# Architecture

Local-first web application: a C++ host owns the HTTP API and job system
and serves the built frontend; mosaic math is the same C++ library. A React
SPA talks to the API with plain `fetch`. One process, one port, everything on
`127.0.0.1`.

Repository layout and system diagrams: [`ARCHITECTURE.md`](../ARCHITECTURE.md).
C++ file map: [`native/README.md`](../native/README.md).

```
┌──────────────────────────── your machine ────────────────────────────┐
│                                                                      │
│  Browser (React SPA)                                                 │
│    │  multipart upload / JSON polling / artifact <img> URLs          │
│    ▼                                                                 │
│  C++ HTTP server (127.0.0.1:8080)              host/http_server.cpp  │
│    │ enqueue                                                         │
│    ▼                                                                 │
│  JobService: bounded queue + worker thread     host/jobs.cpp         │
│    │ run                                                             │
│    ▼                                                                 │
│  runPipeline: sample → match → pack → render   host/pipeline.cpp     │
│    │                                │ writes                         │
│    ▼                                ▼                                │
│  engine (src/engine/)              runtime/jobs/<uuid>/              │
│  data/bricks.db (SQLite catalog)                                     │
└──────────────────────────────────────────────────────────────────────┘
```

## Layers

| Unit | Path | Responsibility |
|---|---|---|
| engine | `native/src/engine/` | Sampler, matcher, packers, renderer. No HTTP. |
| `runPipeline` | `native/src/host/pipeline.cpp` | One full run: sample → match → pack → render |
| `JobService` | `native/src/host/jobs.cpp` | Queue, worker, timeout, piece-target search |
| `loadCatalog` / `FileJobRepository` | `native/src/host/catalog.cpp`, `jobs.cpp` | `bricks.db` once; job dirs + manifests |
| `http_server` / `upload_validator` | `native/src/host/` | Routes, Host check, upload rules |
| `lego_cli` | `native/src/cli_main.cpp` | Same pipeline, no server |

The dependency direction is strictly downward: HTTP code knows about the
pipeline, the pipeline knows nothing about HTTP. That is what lets the CLI and
the tests drive the exact same engine.

## The pipeline

`runPipeline(catalog, JobConfig, progress, cancel)` executes one job:

1. **PREPROCESSING** — decode the (already validated) input, then box-average
   into a stud grid (`toStudGrid` when `targetStudWidth > 0`, else
   `toStudGridByBlockSize`). Classic web jobs use width 54.
2. **MATCHING** — nearest LEGO color per stud via Euclidean OKLab distance
   against the palette of colors that exist as 1×1 plates in `bricks.db`.
3. **PACKING** — for each requested pack mode (enum order: greedy, ilp, rle,
   component, dlx, anneal), tile same-color regions with catalog plates.
4. **RENDERING** — stud-texture previews, BOM text files, and
   `placements-*.json` with every placed plate.

Output is a `PipelineResult` (grid size, color counts, per-mode piece
counts and structured BOM, artifact names) — the UI never parses text files.

## Job lifecycle

```
POST /api/v1/jobs
  └─ store upload → validate → manifest(QUEUED) → worker queue (max 8)
worker thread (1 by default)
  └─ QUEUED → PREPROCESSING → MATCHING → PACKING → RENDERING → COMPLETE
                                                              └→ FAILED
```

- Every state change is persisted by atomically rewriting
  `runtime/jobs/<uuid>/manifest.json` (write temp file, rename), so a
  crash can never leave a half-written manifest.
- A watchdog cancels any job after **120 s** (single mode; compare-all and
  piece-search scale up to 10 min); the packers additionally have their own
  internal time budgets.
- On startup, any manifest still in a non-terminal state is marked FAILED
  ("Interrupted by server restart") — jobs are never silently resumed.
- The queue holds at most 8 jobs; a 9th upload gets HTTP 429.

## Storage

One directory per job, `runtime/jobs/<uuid>/`:

```
manifest.json            source of truth for status/result
input.png                the upload (kept for reruns/debugging)
matched.png              flat matched colors
lego-studs.png           1×1 stud preview
lego-greedy.png          packed preview (per mode)
bom-greedy.txt           human-readable parts list (per mode)
placements-greedy.json   every placed plate (per mode)
color-counts.txt         per-color stud tally
```

Retention (enforced on startup and before each new job): completed jobs older
than **7 days**, or beyond **50 jobs** / **2 GB** total, are deleted oldest
first. Running jobs are never evicted.

## Safety model

The threat model is "app that touches untrusted image files and is reachable
from a browser". Locally it binds to loopback; hosted, it sits behind
Cloudflare (see [`deploy.md`](deploy.md)) and the same guards are configured
by environment variable.

| Risk | Defense | Where |
|---|---|---|
| Remote access | Bind to `127.0.0.1` by default (`LEGO_BIND`); in Docker, no published port — only the proxy container can reach it | `lego_server`, compose |
| DNS rebinding / wrong hostname | Reject any request whose `Host` is not in `LEGO_ALLOWED_HOSTS` (default `localhost,127.0.0.1`) → 403 | `http_server` pre-routing |
| Upload floods | Per-visitor `LEGO_UPLOADS_PER_MINUTE` → 429 with `Retry-After`; visitor IP from `CF-Connecting-IP` / `X-Forwarded-For` when `LEGO_TRUST_PROXY=1` | `http_server`, `RateLimiter` |
| Oversized upload | 25 MB cap on the HTTP body and again while storing the file | `http_server` |
| Content-type spoofing | Magic-byte sniffing (PNG/JPEG signatures); extension and client MIME ignored | `upload_validator` |
| Decompression bomb | Declared dimensions read from the image *header* before any pixel decode; > 25 MP rejected. Decode uses RAII `unique_ptr` + pixel cap. | `upload_validator`, `image_io` |
| Path traversal | Artifact names must appear in the job's manifest whitelist, pass a basename allowlist, and the resolved path must stay inside the job dir; job IDs must parse as UUIDs | `http_server` |
| Stored-file sniffing | `X-Content-Type-Options: nosniff` on artifacts; non-image artifacts served as attachments | `http_server` |
| Runaway job | 120 s (or scaled) watchdog sets a cancel flag checked between stages | `JobService`, `runPipeline` |
| Disk exhaustion | TTL + count + size retention caps | `FileJobRepository` |
| Queue flooding | Bounded queue (8) → HTTP 429 | `JobService` |
| Error detail leaks | Unhandled exceptions log server-side, client sees generic 500 JSON; health endpoint exposes no filesystem paths | `http_server` |
| Container privileges | Image runs as a non-root user (uid 10001) | `Dockerfile` |
| SQLite injection | Parameterized queries only; DB opened read-only | `catalog` |
| Memory / overflow | `int64_t` pixel math, RAII for sqlite/image buffers, no unbounded C-string copies of uploads | engine + host |

## Resource budget

Defaults are sized for a laptop: 1 worker (`LEGO_WORKER_COUNT`), ≤ 25 MP
decoded image (~100 MB as INT_ARGB). The 54-stud sample photo completes in a
few seconds end to end; greedy packing itself is tens of milliseconds —
decode and PNG encode dominate.

## Scaling path (not built, by design)

If this ever needs to be multi-user: the seams are already in place —
`FileJobRepository` becomes object storage + a database, the in-process queue
becomes a real queue, `JobService` workers move to separate processes, and
`loadCatalog` / `runPipeline` move unchanged. Nothing in the engine knows the
storage or transport.

## Configuration

Environment variables, all optional:

| Variable | Default | Meaning |
|---|---|---|
| `LEGO_BIND` | `127.0.0.1` | Interface to listen on; `0.0.0.0` inside a container |
| `LEGO_PORT` | `8080` | HTTP port |
| `LEGO_DB_PATH` | `data/bricks.db` | LEGO catalog SQLite file |
| `LEGO_JOBS_PATH` | `runtime/jobs` | Per-job storage root |
| `LEGO_WEB_DIST` | `web/dist` | Built SPA to serve at `/` (skipped if missing) |
| `LEGO_WORKER_COUNT` | `1` | Concurrent pipeline workers |
| `LEGO_ALLOWED_HOSTS` | `localhost,127.0.0.1` | Comma list of accepted `Host` header values (no port) |
| `LEGO_TRUST_PROXY` | `0` | `1` to take the client IP from `CF-Connecting-IP` / `X-Forwarded-For` |
| `LEGO_UPLOADS_PER_MINUTE` | `0` | Per-client-IP cap on `POST /api/v1/jobs`; `0` disables |

Hosting walkthrough (Cloudflare Tunnel on a home PC, or Caddy on a VM):
[`deploy.md`](deploy.md).

Note for public hosting: the `pieces` sizing mode and the `ilp` / `dlx` /
`anneal` packers can run for minutes and occupy a worker. Consider hiding them
in the UI if strangers abuse them.

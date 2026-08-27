# Architecture

Local-first web application: a Java backend owns all image processing and
serves both the HTTP API and the built frontend; a React SPA talks to it with
plain `fetch`. One process, one port, everything on `127.0.0.1`.

```
┌──────────────────────────── your machine ────────────────────────────┐
│                                                                      │
│  Browser (React SPA)                                                 │
│    │  multipart upload / JSON polling / artifact <img> URLs          │
│    ▼                                                                 │
│  Javalin HTTP server (127.0.0.1:8080)          backend/…/api         │
│    │ enqueue                                                         │
│    ▼                                                                 │
│  JobService: bounded queue + worker thread     backend/…/application │
│    │ run                                                             │
│    ▼                                                                 │
│  PipelineService: sample → match → pack → render                     │
│    │ reads                          │ writes                         │
│    ▼                                ▼                                │
│  data/bricks.db (read-only)      runtime/jobs/<uuid>/ (per job)      │
└──────────────────────────────────────────────────────────────────────┘
```

## Layers (backend packages)

| Package | Responsibility | Depends on |
|---|---|---|
| `core.color`, `core.image`, `core.pack` | JNI wrappers for the C++ engine (same algorithm logic). No HTTP, no job knowledge. | `nativeengine` → `liblegocore` |
| `domain` | Types: `JobConfig`, `JobStatus`, `PackMode`, `JobManifest`, `PipelineResult` | core |
| `application` | `PipelineService` (one full run), `JobService` (queue, worker, timeout) | domain, core |
| `infrastructure` | `CatalogProvider` (bricks.db, loaded once), `FileJobRepository` (job dirs + manifests) | domain |
| `api` | Javalin routes, upload validation, DTO mapping | application |
| `cli` | `CliApplication` — same pipeline, no server | application |

The dependency direction is strictly downward: HTTP code knows about the
pipeline, the pipeline knows nothing about HTTP. That is what lets the CLI and
the tests drive the exact same engine.

## The pipeline

`PipelineService.run(JobConfig, ProgressListener)` executes one job:

1. **PREPROCESSING** — decode the (already validated) input, then box-average
   each `blockSize × blockSize` pixel block into one stud (`ImageSampler`,
   default `blockSize = 80`, same as the old CLI `BLOCK_SIZE`).
2. **MATCHING** — nearest LEGO color per stud via Euclidean RGB distance
   (`ColorMatcher` → C++), against the palette of colors that exist as 1×1 plates in
   `bricks.db`.
3. **PACKING** — for each requested `PackMode`, tile same-color regions with
   catalog plates. See [`packing.md`](packing.md) and
   [`algorithm-walkthrough.md`](algorithm-walkthrough.md).
4. **RENDERING** — stud-texture previews (`LegoRenderer`), BOM text files, and
   `placements-*.json` with every placed plate.

Output is a `PipelineResult` record (grid size, color counts, per-mode piece
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
  `runtime/jobs/<uuid>/manifest.json` (write temp file, `ATOMIC_MOVE`), so a
  crash can never leave a half-written manifest.
- A watchdog cancels any job after **120 s** (`JobService.JOB_TIMEOUT`); the
  packers additionally have their own internal time budgets.
- On startup, any manifest still in a non-terminal state is marked FAILED
  ("interrupted by backend restart") — jobs are never silently resumed.
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

The threat model is "local app that touches untrusted image files and is
reachable from a browser":

| Risk | Defense | Where |
|---|---|---|
| Remote access | Bind to `127.0.0.1` only | `Application.start` |
| DNS rebinding (evil website → localhost API) | Reject any request whose `Host` is not `localhost`/`127.0.0.1` → 403 | `Application` before-filter |
| Oversized upload | 25 MB cap enforced while streaming (Jetty multipart config + copy loop) | `Application`, `JobRoutes` |
| Content-type spoofing | Magic-byte sniffing (PNG/JPEG signatures); extension and client MIME ignored | `UploadValidator` |
| Decompression bomb | Declared dimensions read from the image *header* before any pixel decode; > 25 MP rejected | `UploadValidator` |
| Path traversal | Artifact names must appear in the job's manifest whitelist, and the resolved path must stay inside the job dir; job IDs must parse as UUIDs | `JobRoutes` |
| Stored-file sniffing | `X-Content-Type-Options: nosniff` on artifacts; non-image artifacts served as attachments | `JobRoutes` |
| Runaway job | 120 s watchdog cancels the pipeline thread; interruption checked between stages | `JobService`, `PipelineService` |
| Disk exhaustion | TTL + count + size retention caps | `FileJobRepository` |
| Queue flooding | Bounded queue (8) → HTTP 429 | `JobService` |
| Error detail leaks | Unhandled exceptions log server-side, client sees generic 500 JSON | `Application` exception mappers |

## Resource budget

Defaults are sized for a laptop: 1 worker (`LEGO_WORKER_COUNT`), ≤ 25 MP
decoded image (~100 MB as INT_ARGB), `-Xmx1g` in `make start`. The 54-stud
sample photo completes in ~3 s end to end; greedy packing itself is ~25 ms —
decode and PNG encode dominate.

## Scaling path (not built, by design)

If this ever needs to be multi-user: the seams are already in place — 
`FileJobRepository` becomes object storage + a database, the in-process queue
becomes a real queue, `JobService` workers move to separate processes, and
`CatalogProvider`/`PipelineService` move unchanged. Nothing in `core` or
`application` knows the storage or transport.

## Configuration

Environment variables, all optional:

| Variable | Default | Meaning |
|---|---|---|
| `LEGO_PORT` | `8080` | HTTP port (always bound to 127.0.0.1) |
| `LEGO_DB_PATH` | `data/bricks.db` | LEGO catalog SQLite file |
| `LEGO_JOBS_PATH` | `runtime/jobs` | Per-job storage root |
| `LEGO_WORKER_COUNT` | `1` | Concurrent pipeline workers |

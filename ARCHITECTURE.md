# Architecture

Local-first LEGO mosaic app: a **C++** host owns the HTTP API, job system,
catalog, and mosaic engine (`native/`); a **React** SPA is the UI. Everything
runs on your machine (`127.0.0.1`).

Deeper ops notes (safety table, retention, env vars): [`docs/architecture.md`](docs/architecture.md).  
HTTP contract: [`docs/api.md`](docs/api.md).

---

## System overview

```mermaid
flowchart LR
  subgraph machine["Your machine"]
    Browser["Browser<br/>React SPA"]
    API["C++ HTTP API<br/>127.0.0.1:8080"]
    Jobs["JobService<br/>queue + worker"]
    Pipe["runPipeline"]
    Native["C++ engine<br/>sample · match · pack · render"]
    DB[("data/bricks.db")]
    Disk[("runtime/jobs/&lt;uuid&gt;/")]

    Browser -->|"POST /api/v1/jobs<br/>GET status / artifacts"| API
    API --> Jobs
    Jobs --> Pipe
    Pipe --> Native
    Pipe -->|palette + plates| DB
    Pipe -->|input + PNGs + BOM| Disk
    API -->|serve artifacts| Disk
  end
```

Production (`make start`): one C++ process serves both `/api/*` and the built `web/dist` UI.  
Development: Vite on `:5173` proxies `/api` → `lego_server` on `:8080`.

---

## Repository layout

```mermaid
flowchart TB
  root["LegoPictureGenerator/"]
  root --> native["native/<br/>C++17 · API + CLI + engine"]
  root --> web["web/<br/>React · TypeScript · Vite"]
  root --> docs["docs/<br/>API, packing, algorithms"]
  root --> data["data/<br/>bricks.db catalog"]
  root --> samples["samples/<br/>example photos"]
  root --> runtime["runtime/<br/>per-job files · gitignored"]
```

| Path | Role |
|------|------|
| `native/` | HTTP host, jobs, CLI, mosaic engine (see [`docs/native.md`](docs/native.md)) |
| `web/` | Upload UI, progress, previews, BOM table |
| `docs/` | Long-form docs and algorithm walkthroughs |
| `data/bricks.db` | Rebrickable-derived color + part availability |
| `runtime/jobs/` | Uploads and outputs for web jobs |

---

## Host layers

Dependencies point **downward only**. HTTP never lives inside packing math; the CLI reuses the same pipeline.

```mermaid
flowchart TB
  subgraph entry["Entry points"]
    App["lego_server"]
    Cli["lego_cli"]
  end

  subgraph api_layer["http"]
    Routes["http_server"]
    Upload["upload_validator"]
  end

  subgraph app_layer["jobs + pipeline"]
    JobSvc["JobService"]
    PipeSvc["runPipeline"]
  end

  subgraph infra["infrastructure"]
    Catalog["loadCatalog"]
    Repo["FileJobRepository"]
  end

  subgraph domain_layer["types"]
    Types["JobConfig · JobManifest · PipelineResult"]
  end

  subgraph engine["engine"]
    Image["image_sampler · renderer"]
    Color["color_matcher"]
    Pack["packers"]
  end

  App --> Routes
  App --> JobSvc
  App --> Catalog
  App --> Repo
  Cli --> PipeSvc
  Cli --> Catalog
  Routes --> JobSvc
  Routes --> Upload
  JobSvc --> PipeSvc
  JobSvc --> Repo
  PipeSvc --> Catalog
  PipeSvc --> Image
  PipeSvc --> Color
  PipeSvc --> Pack
  JobSvc --> Types
  PipeSvc --> Types
  Repo --> Types
```

| Unit | Responsibility |
|---------|----------------|
| `http_server` | Routes, Host check, JSON errors, SPA files |
| `JobService` | Queue, timeouts, piece-target search |
| `runPipeline` | One full mosaic run + artifact writes |
| `FileJobRepository` | Job folders + atomic manifests + retention |
| `loadCatalog` | Palette + plate footprints from `bricks.db` |
| `image_sampler` / `color_matcher` / `packers` / `renderer` | Engine (same logic as the original Java) |
| `lego_cli` | Same pipeline, no server |

---

## Request lifecycle (website)

```mermaid
sequenceDiagram
  participant UI as React UI
  participant API as http_server
  participant JS as JobService
  participant PS as runPipeline
  participant FS as runtime/jobs/uuid

  UI->>API: POST /api/v1/jobs (multipart image)
  API->>FS: write input.png
  API->>API: UploadValidator (magic bytes, size, megapixels)
  API->>JS: submit + enqueue
  API-->>UI: 202 { jobId }

  loop every ~750ms
    UI->>API: GET /api/v1/jobs/{id}
    API-->>UI: status (+ result when done)
  end

  JS->>PS: runPipeline(cfg)
  PS->>FS: matched.png, lego-*.png, bom-*.txt, …
  PS-->>JS: PipelineResult
  JS->>FS: update manifest.json → COMPLETE

  UI->>API: GET .../artifacts/lego-greedy.png
  API-->>UI: PNG bytes
```

Job status machine:

```mermaid
stateDiagram-v2
  [*] --> QUEUED
  QUEUED --> PREPROCESSING
  PREPROCESSING --> MATCHING
  MATCHING --> PACKING
  PACKING --> RENDERING
  RENDERING --> COMPLETE
  QUEUED --> FAILED
  PREPROCESSING --> FAILED
  MATCHING --> FAILED
  PACKING --> FAILED
  RENDERING --> FAILED
  COMPLETE --> [*]
  FAILED --> [*]
```

Guards worth knowing:

- Queue capacity **8** → HTTP **429** when full
- Per-job timeout **120s**
- Restart marks in-flight jobs **FAILED** (never resume half-done work)
- Artifact URLs only serve filenames listed in that job’s manifest

---

## Pipeline (photo → mosaic)

Fixed for now: **`blockSize = 80`** (legacy CLI `BLOCK_SIZE`) — each 80×80 pixel block becomes one stud.

```mermaid
flowchart LR
  In["input.png"] --> Sample["ImageSampler<br/>blockSize 80"]
  Sample --> Grid["stud grid image"]
  Grid --> Match["ColorMatcher<br/>nearest LEGO color"]
  Match --> Studs["studs[][] + matched.png"]
  Studs --> Pack["Packer<br/>default: greedy"]
  Pack --> Placed["List of PlacedPart"]
  Studs --> Render["LegoRenderer"]
  Placed --> Render
  Render --> Out["lego-studs.png<br/>lego-greedy.png"]
  Placed --> Bom["PackBom"]
  Bom --> List["bom-greedy.txt<br/>+ JSON BOM in result"]
```

Web UI defaults to **greedy**; compare-all and other modes are selectable. **Aim for N pieces** forces **DLX** with a multi-probe search. Algorithm detail: [`docs/algorithm-walkthrough.md`](docs/algorithm-walkthrough.md) · [`docs/sizing/MATH.md`](docs/sizing/MATH.md).

---

## Website vs CLI

```mermaid
flowchart TB
  subgraph web_path["Website"]
    W1["Browser upload"] --> W2["http_server + JobService"]
    W2 --> W3["runPipeline"]
    W3 --> W4["runtime/jobs/&lt;uuid&gt;/"]
  end

  subgraph cli_path["CLI · make cli"]
    C1["lego_cli args"] --> C3["runPipeline"]
    C3 --> C4["runtime/cli/ or custom dir"]
  end
```

Same engine (`runPipeline` + packers). The website adds validation, a queue, polling, and artifact URLs.

---

## Per-job storage

```text
runtime/jobs/<uuid>/
  manifest.json           status, settings, PipelineResult, artifact map
  input.png               original upload
  matched.png             flat color-matched grid
  lego-studs.png          every stud as 1×1
  lego-greedy.png         packed plates
  bom-greedy.txt          shopping list
  placements-greedy.json  every plate (x, y, w, h, part, color)
  color-counts.txt        per-color stud tallies
```

`manifest.json` is written atomically (temp file + rename). Retention: completed jobs older than 7 days, or past 50 jobs / ~2 GB, are evicted oldest-first.

---

## Where to change things

| Goal | Start here |
|------|------------|
| HTTP / upload rules | `native/src/http_server.cpp`, `upload_validator.cpp` |
| Queue / timeout | `native/src/jobs.cpp` |
| Mosaic steps | `native/src/pipeline.cpp` |
| Engine / CUDA next | `native/` · [`docs/native.md`](docs/native.md) |
| Block size default | `native/include/lego/types.hpp` |
| Color matching | `native/src/color_matcher.cpp` + `catalog.cpp` |
| Packing | `native/src/packers.cpp` |
| Preview look | `native/src/renderer.cpp` |
| UI pages | `web/src/pages/`, `web/src/styles/app.css` |

---

## Related docs

- [`README.md`](README.md) — how to run
- [`docs/architecture.md`](docs/architecture.md) — safety, retention, config
- [`docs/api.md`](docs/api.md) — endpoints
- [`docs/image.md`](docs/image.md) · [`docs/color.md`](docs/color.md) · [`docs/packing.md`](docs/packing.md)
- [`docs/MATH.md`](docs/MATH.md) — formula index (sampling, color, sizing, packing)
- [`docs/native.md`](docs/native.md) — C++ engine and host

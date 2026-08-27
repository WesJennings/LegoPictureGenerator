# Architecture

Local-first LEGO mosaic app: a **Java** host owns the HTTP API and job system;
a **C++** engine (`native/`, JNI) runs sampling, matching, packing, and
rendering; a **React** SPA is the UI. Everything runs on your machine (`127.0.0.1`).

Deeper ops notes (safety table, retention, env vars): [`docs/architecture.md`](docs/architecture.md).  
HTTP contract: [`docs/api.md`](docs/api.md).

---

## System overview

```mermaid
flowchart LR
  subgraph machine["Your machine"]
    Browser["Browser<br/>React SPA"]
    API["Javalin API<br/>127.0.0.1:8080"]
    Jobs["JobService<br/>queue + worker"]
    Pipe["PipelineService"]
    Native["C++ engine<br/>JNI liblegocore"]
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

Production (`make start`): one Java process serves both `/api/*` and the built `web/dist` UI.  
Development: Vite on `:5173` proxies `/api` → the Java backend on `:8080`.

---

## Repository layout

```mermaid
flowchart TB
  root["LegoPictureGenerator/"]
  root --> backend["backend/<br/>Java 21 · Maven · API + CLI + JNI"]
  root --> native["native/<br/>C++17 · sample/match/pack/render"]
  root --> web["web/<br/>React · TypeScript · Vite"]
  root --> docs["docs/<br/>API, packing, algorithms"]
  root --> data["data/<br/>bricks.db catalog"]
  root --> samples["samples/<br/>example photos"]
  root --> runtime["runtime/<br/>per-job files · gitignored"]
```

| Path | Role |
|------|------|
| `backend/` | Job system, Javalin API, CLI, JNI wrappers around the engine |
| `native/` | C++ mosaic engine (see [`docs/native.md`](docs/native.md)) |
| `web/` | Upload UI, progress, previews, BOM table |
| `docs/` | Long-form docs and algorithm walkthroughs |
| `data/bricks.db` | Rebrickable-derived color + part availability |
| `runtime/jobs/` | Uploads and outputs for web jobs |

---

## Backend layers

Dependencies point **downward only**. HTTP never lives inside packing math; the CLI reuses the same pipeline.

```mermaid
flowchart TB
  subgraph entry["Entry points"]
    App["Application<br/>web server"]
    Cli["CliApplication<br/>offline"]
  end

  subgraph api_layer["api"]
    Routes["JobRoutes"]
    Upload["UploadValidator"]
  end

  subgraph app_layer["application"]
    JobSvc["JobService"]
    PipeSvc["PipelineService"]
  end

  subgraph infra["infrastructure"]
    Catalog["CatalogProvider"]
    Repo["FileJobRepository"]
  end

  subgraph domain_layer["domain"]
    Types["JobConfig · JobStatus · PackMode<br/>JobManifest · PipelineResult"]
  end

  subgraph core_layer["core — JNI wrappers"]
    Image["image: ImageSampler, LegoRenderer"]
    Color["color: ColorMatcher"]
    Pack["pack: GreedyPacker, ExactIlpPacker, …"]
    Native["nativeengine: NativeEngine"]
  end

  subgraph cpp["native/ C++"]
    Cpp["liblegocore<br/>sample · match · pack · render"]
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
  Image --> Native
  Color --> Native
  Pack --> Native
  Native --> Cpp
  JobSvc --> Types
  PipeSvc --> Types
  Repo --> Types
```

| Package | Responsibility |
|---------|----------------|
| `api` | HTTP routes, upload validation, JSON responses |
| `application` | Job queue/timeouts; one full mosaic run |
| `infrastructure` | Load `bricks.db` once; job folders + manifests |
| `domain` | Shared types (no I/O) |
| `core.*` | Java wrappers: sampling, color match, pack, render |
| `core.nativeengine` | JNI load + calls into `liblegocore` |
| `native/` (C++) | Algorithm implementations — same logic as the original Java |
| `cli` | Same `PipelineService`, no server |

---

## Request lifecycle (website)

```mermaid
sequenceDiagram
  participant UI as React UI
  participant API as JobRoutes
  participant JS as JobService
  participant PS as PipelineService
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

  JS->>PS: run(JobConfig)
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
    W1["Browser upload"] --> W2["JobRoutes + JobService"]
    W2 --> W3["PipelineService"]
    W3 --> W4["runtime/jobs/&lt;uuid&gt;/"]
  end

  subgraph cli_path["CLI · make cli"]
    C1["CliApplication args"] --> C3["PipelineService"]
    C3 --> C4["runtime/cli/ or custom dir"]
  end
```

Same engine (`PipelineService` + `core.*`). The website adds validation, a queue, polling, and artifact URLs.

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
| HTTP / upload rules | `backend/.../api/JobRoutes.java`, `UploadValidator.java` |
| Queue / timeout | `backend/.../application/JobService.java` |
| Mosaic steps | `backend/.../application/PipelineService.java` |
| C++ engine / CUDA next | `native/` · [`docs/native.md`](docs/native.md) |
| Block size default | `domain/JobConfig.DEFAULT_BLOCK_SIZE` |
| Color matching | `core/color/ColorMatcher.java` |
| Packing | `core/pack/*Packer.java` |
| Preview look | `core/image/LegoRenderer.java` |
| UI pages | `web/src/pages/`, `web/src/styles/app.css` |

---

## Related docs

- [`README.md`](README.md) — how to run
- [`docs/architecture.md`](docs/architecture.md) — safety, retention, config
- [`docs/api.md`](docs/api.md) — endpoints
- [`docs/image.md`](docs/image.md) · [`docs/color.md`](docs/color.md) · [`docs/packing.md`](docs/packing.md)
- [`docs/MATH.md`](docs/MATH.md) — formula index (sampling, color, sizing, packing)
- [`docs/native.md`](docs/native.md) — C++ engine and JNI

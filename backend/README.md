# Backend

Java 21 Maven host for the website: job system, HTTP API, CLI, and JNI
wrappers around the C++ mosaic engine (`../native/`). Algorithm logic lives
in C++; see [`../docs/native.md`](../docs/native.md).

## Build & run

```bash
./mvnw test                 # unit + integration tests
./mvnw package -DskipTests  # shaded JAR in target/
java -jar target/lego-picture-generator-0.1.0.jar   # run from the repo root
```

Run from the **repo root** (or set `LEGO_DB_PATH` / `LEGO_JOBS_PATH`) so the
default relative paths resolve. `make dev` / `make start` at the repo root do
this for you. Maven itself is bootstrapped by `./mvnw` — no install needed.

## Package map

```
com.legopicturegenerator
├── Application.java        # web entrypoint: wiring, Javalin, exception mapping
├── config/AppConfig        # env vars → typed config
├── api/                    # HTTP layer
│   ├── JobRoutes           # POST/GET/DELETE /api/v1/jobs, artifact serving
│   └── UploadValidator     # magic bytes, size cap, header-only dimension check
├── application/            # use cases
│   ├── PipelineService     # one full run: sample → match → pack → render
│   └── JobService          # bounded queue, worker, 120s watchdog, manifest updates
├── domain/                 # types: JobConfig, JobStatus, PackMode,
│                           #        JobManifest, PipelineResult
├── infrastructure/
│   ├── CatalogProvider     # loads palette + plate catalog from bricks.db once
│   └── FileJobRepository   # job dirs, atomic manifest writes, retention
├── core/                   # JNI wrappers — algorithms in native/
│   ├── color/ColorMatcher      # JDBC load + JNI Euclidean nearest-color
│   ├── image/ImageSampler      # JNI box-average photo → stud grid
│   ├── image/LegoRenderer      # JNI stud-texture + packed previews
│   ├── image/PieceCountFormatter
│   ├── nativeengine/NativeEngine
│   └── pack/                   # JNI wrappers: GreedyPacker, ExactIlpPacker, …
└── cli/CliApplication      # same pipeline without a server
```

Rule of thumb: `core` depends on nothing above it; `api` never touches files
or algorithms directly. Details in
[`../docs/architecture.md`](../docs/architecture.md).

## CLI

```bash
# from the repo root
make cli
# or explicitly:
cd backend && LEGO_DB_PATH=../data/bricks.db ./mvnw -q compile exec:java \
  -Dexec.mainClass=com.legopicturegenerator.cli.CliApplication \
  -Dexec.args="../samples/Jarvis.png ../runtime/cli 54 greedy,ilp,dlx"
```

Args (all optional, in order): input image, output dir, stud width (16–128),
comma-separated modes.

## Tests

- `NativeParityTest` — C++ engine: sampling, matching, all six packers cover
  a split grid, render dimensions
- `ImageSamplerTest` — aspect ratio, clamping, full-coverage averaging
- `UploadValidatorTest` — fake/truncated/oversized files rejected before decode
- `PipelineServiceTest` — end-to-end run against a tiny in-test SQLite fixture;
  asserts BOM totals equal piece counts and all artifacts exist

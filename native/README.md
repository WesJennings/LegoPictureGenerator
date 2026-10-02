# native/

C++17 mosaic **engine** and local **host** (HTTP API, jobs, CLI).
Build with `bash native/build.sh` (or `make setup` from the repo root).

Requires a C++17 compiler, CMake, and SQLite 3. No JDK.

Repo-wide diagrams: [`../ARCHITECTURE.md`](../ARCHITECTURE.md).
Parity notes and CUDA seam: [`../docs/native.md`](../docs/native.md).

## Layout

```text
native/
├── CMakeLists.txt       # liblegocore + lego_server / lego_cli / tests
├── build.sh             # cmake + build + run tests
├── include/lego/        # public headers (one per module)
├── src/
│   ├── engine/          # mosaic math — no HTTP, no jobs
│   ├── host/            # catalog, pipeline, jobs, HTTP
│   ├── server_main.cpp  # lego_server
│   └── cli_main.cpp     # lego_cli
├── tests/
│   ├── native_tests.cpp # sampler / packer / RNG
│   └── host_tests.cpp   # upload, pipeline, jobs, HTTP contract
└── third_party/         # cpp-httplib, nlohmann/json, stb
```

Headers stay flat under `include/lego/` so includes are `#include "lego/jobs.hpp"`.
Sources are split so engine code cannot grow HTTP knowledge by accident.

### Engine — `src/engine/`

| File | Role |
|------|------|
| `image_sampler.cpp` | Photo → stud grid (box average, aspect-preserving width) |
| `color_matcher.cpp` | Nearest LEGO color (Euclidean OKLab) |
| `packers.cpp` | Six packers: greedy, ILP, RLE, component, DLX, anneal |
| `renderer.cpp` | Stud-texture and packed-plate PNG pixels |

### Host — `src/host/`

| File | Role |
|------|------|
| `catalog.cpp` | Load palette + plate footprints from `bricks.db` |
| `image_io.cpp` | PNG/JPEG decode and PNG encode (stb) |
| `pipeline.cpp` | One full run: sample → match → pack → render + artifacts |
| `jobs.cpp` | Queue, worker, timeouts, retention, `FileJobRepository` |
| `http_server.cpp` | Routes, Host check, SPA files, artifact serving |
| `upload_validator.cpp` | Magic bytes, size, megapixel cap |
| `piece_target.cpp` | “Aim for N pieces” DLX search |
| `text.cpp` | BOM / color-count formatting |
| `config.cpp` | Env vars (`LEGO_PORT`, `LEGO_DB_PATH`, …) |

## Binaries

After `bash native/build.sh`:

| Binary | Role |
|--------|------|
| `native/build/lego_server` | HTTP API + optional `web/dist` on `127.0.0.1` |
| `native/build/lego_cli` | Same pipeline, no server |
| `native/build/lego_native_tests` | Sampler / packer / RNG |
| `native/build/lego_host_tests` | Upload, pipeline, jobs, HTTP contract |

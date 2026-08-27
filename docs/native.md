# Native C++ host and engine

Sampling, color matching, packing, rendering, the HTTP API, job queue, SQLite
catalog load, PNG I/O, and the CLI all run in **C++** (`native/`). The React
UI is unchanged and talks to the same `/api/v1` contract as before.

Packers keep the **same control flow** as the original Java (including DLX/ILP
time budgets and `Random(42)` for annealing). Pixel kernels
(`averageRegion` / `nearestIndex`) are isolated C++ functions, which is the
CUDA seam later — not a rewrite of the website.

## Layout

```text
native/
  include/lego/     public C++ headers
  src/              engine + host (HTTP, jobs, pipeline, CLI/server mains)
  tests/            engine checks + host/API checks (no JVM)
  third_party/      cpp-httplib, nlohmann/json, stb
  build.sh          cmake + run tests
```

Binaries (after `bash native/build.sh`):

| Binary | Role |
|---|---|
| `native/build/lego_server` | HTTP API + optional `web/dist` SPA on `127.0.0.1` |
| `native/build/lego_cli` | Same pipeline, no server |
| `native/build/lego_native_tests` | Sampler / packer / RNG checks |
| `native/build/lego_host_tests` | Upload, pipeline, jobs, HTTP contract |

## What stayed the same

| Stage | Logic |
|---|---|
| Box-average sample | Integer mean RGB, `floor(w/B)×floor(h/B)` block path, aspect-preserving width path |
| Color match | Euclidean RGB, first-best on ties, palette order from `bricks.db` |
| Greedy / RLE / component | Same scan orders, repair, flood-fill neighbor order |
| ILP / DLX | 64-cell bitmasks, same lower bound, same branching (lowest bit vs fewest options) |
| Anneal | Seed 42 via OpenJDK `java.util.Random` LCG, same window size / cooling |
| Catalog | Parameterized SQLite; footprints-for-color lists largest-first |
| HTTP / jobs | Same routes, JSON shapes, queue, timeouts, retention, Host check |

## What is allowed to differ

- **Elapsed milliseconds** in BOM / compare text (C++ is often faster).
- **Knob antialiasing** in packed PNGs: software ellipse rasterizer instead of Java2D. Plate layout, shade factors (0.62 / 0.88 / 1.18 / 0.85 / 1.35), and geometry match.
- ILP/DLX on huge blobs that hit the wall-clock budget may search farther in the same 30s/60s because C++ is faster; small mosaics (and tests) finish to the same optimum.
- PNG encoder bytes (stb vs ImageIO) — pixel values match; compressed files may not be byte-identical.

## Build

CMake + a C++17 compiler + SQLite 3 (`libsqlite3-dev`). No JDK.

```bash
bash native/build.sh
make test
```

## CUDA later

Do **not** rewrite the website. Add `.cu` kernels behind the existing C++
functions:

1. `averageRegion` / `toStudGrid*`
2. `nearestIndex` / `matchImage`

Leave packers on CPU.

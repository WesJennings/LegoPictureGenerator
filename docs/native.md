# Native C++ engine

Sampling, color matching, packing, and LEGO-look rendering run in **C++**
(`native/`) and are called from Java through JNI. The HTTP API, job queue,
SQLite catalog load, and React UI stay in Java/TypeScript.

This is the first step toward CUDA: the pixel kernels (`ImageSampler`,
`ColorMatcher`) are already isolated C++ functions. Packers stay on the CPU
with the **same control flow** as the original Java (including DLX/ILP time
budgets and `Random(42)` for annealing).

## Layout

```text
native/
  include/lego/     public C++ headers
  src/              sampler, matcher, packers, renderer, JNI
  tests/            standalone C++ checks (no JVM)
  build.sh          cmake + copy liblegocore.so into the Java resources tree
```

Java entry: `com.legopicturegenerator.core.nativeengine.NativeEngine`.

The six packer classes (`GreedyPacker`, …) are thin wrappers around
`NativeEngine.pack`. They keep the same public API so `PipelineService` and
tests did not change.

## What stayed the same

| Stage | Logic |
|---|---|
| Box-average sample | Integer mean RGB, `floor(w/B)×floor(h/B)` block path, aspect-preserving width path |
| Color match | Euclidean RGB, first-best on ties, palette order from `bricks.db` |
| Greedy / RLE / component | Same scan orders, repair, flood-fill neighbor order |
| ILP / DLX | 64-cell bitmasks, same lower bound, same branching (lowest bit vs fewest options) |
| Anneal | Seed 42 via OpenJDK `java.util.Random` LCG, same window size / cooling |
| Catalog | Java still loads SQLite; footprints-for-color lists are passed into C++ in the same largest-first order |

## What is allowed to differ

- **Elapsed milliseconds** in BOM / compare text (C++ is often faster).
- **Knob antialiasing** in packed PNGs: C++ uses a software ellipse rasterizer instead of Java2D. Plate layout, shade factors (0.62 / 0.88 / 1.18 / 0.85 / 1.35), and geometry match.
- ILP/DLX on huge blobs that hit the wall-clock budget may search farther in the same 30s/60s because C++ is faster; small mosaics (and tests) finish to the same optimum.

## Build

CMake + a C++17 compiler. `JAVA_HOME` must point at a JDK (JNI headers).

```bash
bash native/build.sh     # also run automatically by Maven generate-resources
make test                # native tests + JUnit + frontend type-check
```

The shared library is `native/build/liblegocore.so`, copied to
`backend/src/main/resources/native/` so the shaded JAR can extract it.

Override the library path with `LEGO_NATIVE_LIB` or `-Dlego.native.lib=...`.

## CUDA later

Do **not** rewrite the website. Add `.cu` kernels behind the existing C++
functions:

1. `averageRegion` / `toStudGrid*`
2. `nearestIndex` / `matchImage`

Leave packers on CPU. See the root README and `ARCHITECTURE.md`.

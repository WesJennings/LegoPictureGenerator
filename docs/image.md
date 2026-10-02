# Image sampling & rendering

How a photo becomes a stud grid, and how results are drawn to look like LEGO.
Averaging and drawing loops are C++ (`native/src/engine/image_sampler.cpp`,
`native/src/engine/renderer.cpp`). See [native.md](native.md).

See also: [sampling/MATH.md](sampling/MATH.md) · [sizing/MATH.md](sizing/MATH.md).

## Files

| File | Role |
|------|------|
| `engine/image_sampler.cpp` | Photo → stud grid (box averaging, aspect-preserving) |
| `engine/renderer.cpp` | Procedural stud / packed-plate PNG pixels (no file I/O) |
| `host/text.cpp` | Format color tallies into a shopping-list report |
| `host/image_io.cpp` | Decode upload / encode PNG artifacts |

The pipeline that wires these together is
`native/src/host/pipeline.cpp`; per-job outputs land in
`runtime/jobs/<uuid>/` (web) or the directory you pass to the CLI.

## Sampling

Replaces the old fixed `BLOCK_SIZE` downscale. You choose the **output** size
(`targetStudWidth`, 16–128 studs); the sampler derives the height from the
source aspect ratio and box-averages each output cell over its proportional
source region:

```text
input photo (w×h pixels)
        │  targetStudWidth = 54
        ▼
grid width  = min(54, w)
grid height = max(1, round(h · 54 / w))       # aspect preserved
each cell   = mean RGB of its source block    # every pixel counted once
```

Properties worth knowing:

- No input can produce a zero-sized grid (dimensions clamp to ≥ 1).
- Sources smaller than the target aren't upscaled — the grid clamps to the
  source size.
- Non-divisible dimensions are handled by proportional block edges, so edge
  rows/columns are never dropped.

Entry points: `toStudGrid`, `toStudGridByBlockSize` (`image_sampler.cpp`).

## Rendering

Pure functions — the caller writes files (`image_io.cpp`).

| Function | Draws |
|----------|--------|
| `renderStuds(gridArgb, cols, rows, studSizePx)` | Every stud as its own 1×1 visual plate + knob |
| `renderPacked(gridArgb, cols, rows, studSizePx, placed)` | Multi-stud plates as one continuous body + knobs per stud |

Colors for packed plates are sampled from the stud grid at each part's origin.
Default stud size is 24 px (`DEFAULT_RENDER_STUD_PX` in `types.hpp`).

## Color-count report

`formatColorCounts` in `text.cpp` does **not** re-scan the grid. It formats
maps already filled during `matchImage`: total studs (= 1×1 piece count before
packing) and per-color lines sorted by count. Written to `color-counts.txt`
per job.

## Run

From the repo root: `make start` (web) or `make cli` (offline). See the
[root README](../README.md).

## Related docs

- [`color.md`](color.md) — matching
- [`packing.md`](packing.md) — packing overview + research · [`algorithm-walkthrough.md`](algorithm-walkthrough.md) — algorithm details
- [`../ARCHITECTURE.md`](../ARCHITECTURE.md) — layout and pipeline
- [`architecture.md`](architecture.md) — ops: safety, retention, config

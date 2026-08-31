# Image sampling & rendering

How a photo becomes a stud grid, and how results are drawn to look like LEGO.
Averaging and drawing loops are C++ (`native/src/image_sampler.cpp`,
`native/src/renderer.cpp`). See [native.md](native.md).

See also: [sampling/MATH.md](sampling/MATH.md) · [sizing/MATH.md](sizing/MATH.md).

## Files

| File | Role |
|------|------|
| `image_sampler.cpp` | Photo → stud grid (box averaging, aspect-preserving) |
| `renderer.cpp` | Procedural stud / packed-plate PNGs (no file I/O) |
| `text.cpp` | Format color tallies and BOMs into shopping-list reports |

The pipeline that wires these together is
`native/src/pipeline.cpp`; per-job outputs land in
`runtime/jobs/<uuid>/` (web) or the directory you pass to the CLI.

## Sampling (`image_sampler.cpp`)

Two entry points:

| Function | When used |
|----------|-----------|
| `toStudGrid(src, sw, sh, targetStudWidth)` | Classic / stud-width sizing (`targetStudWidth` 16–128; web classic ≈54) |
| `toStudGridByBlockSize(src, sw, sh, blockSize)` | Block-size path (CLI default `80`; piece-target search probes) |

`toStudGrid` derives height from the source aspect ratio and box-averages each
output cell over its proportional source region:

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

## Rendering (`renderer.cpp`)

Pure functions returning ARGB buffers — the caller (`pipeline.cpp`) writes PNGs.

| Function | Draws |
|----------|--------|
| `renderStuds(gridArgb, cols, rows, studSizePx)` | Every stud as its own 1×1 visual plate + knob → `lego-studs.png` |
| `renderPacked(gridArgb, cols, rows, studSizePx, placed)` | Multi-stud plates as one continuous body + knobs per stud → `lego-<mode>.png` |

Colors for packed plates are sampled from `gridArgb` at each part's origin.
Default stud size is 24 px (`DEFAULT_RENDER_STUD_PX`).

The flat matched mosaic (no stud texture) is written separately as
`matched.png` from `matchImage`'s ARGB buffer — that is not a renderer call.

## Color / BOM text (`text.cpp`)

Does **not** re-scan the grid. Formats maps already filled during matching:
total studs (= 1×1 piece count before packing) and per-color lines sorted by
count → `color-counts.txt`. Packed shopping lists use `formatBom`; optional
1×1 stud lists use `formatStudBom` → `bom-studs.txt`.

## Run

From the repo root: `make start` (web) or `make cli` (offline). See the
[root README](../README.md).

## Related docs

- [`color.md`](color.md) — matching
- [`packing.md`](packing.md) — packing overview + research · [`algorithm-walkthrough.md`](algorithm-walkthrough.md) — algorithm details
- [`architecture.md`](architecture.md) — full pipeline and job system

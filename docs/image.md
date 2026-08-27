# Image sampling & rendering

How a photo becomes a stud grid, and how results are drawn to look like LEGO.
Java adapters live in `backend/src/main/java/com/legopicturegenerator/core/image/`;
the averaging and drawing loops are C++ (`native/src/image_sampler.cpp`,
`native/src/renderer.cpp`). See [native.md](native.md).

See also: [sampling/MATH.md](sampling/MATH.md) · [sizing/MATH.md](sizing/MATH.md).

## Files

| Class | Role |
|------|------|
| `ImageSampler` | Photo → stud grid (box averaging, aspect-preserving) |
| `LegoRenderer` | Procedural stud / packed-plate PNGs (no file I/O) |
| `PieceCountFormatter` | Format color tallies into a shopping-list report |

The pipeline that wires these together is
`application/PipelineService.java`; per-job outputs land in
`runtime/jobs/<uuid>/` (web) or the directory you pass to the CLI.

## `ImageSampler`

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

## `LegoRenderer`

Pure functions — the caller writes files.

| Method | Draws |
|--------|--------|
| `renderStuds(BufferedImage, studSizePx)` | Every stud as its own 1×1 visual plate + knob |
| `renderPacked(List<PlacedPart>, studs, studSizePx)` | Multi-stud plates as one continuous body + knobs per stud |
| `renderPacked(PackResult, studs, studSizePx)` | Same, from a pack result |

Colors for packed plates are sampled from `studs[y][x]` at each part's origin.
Default stud size is 24 px (`JobConfig.DEFAULT_RENDER_STUD_PX`).

## `PieceCountFormatter`

Does **not** re-scan the grid. Formats maps already filled during
`ColorMatcher.matchImage`: total studs (= 1×1 piece count before packing) and
per-color lines sorted by count. Written to `color-counts.txt` per job.

## Run

From the repo root: `make start` (web) or `make cli` (offline). See the
[root README](../README.md).

## Related docs

- [`color.md`](color.md) — matching
- [`packing.md`](packing.md) — packing overview + research · [`algorithm-walkthrough.md`](algorithm-walkthrough.md) — algorithm details
- [`architecture.md`](architecture.md) — full pipeline and job system

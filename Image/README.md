# Image

Main pipeline entry, stud downscaling, 1×1 piece reports, and 2D LEGO-look rendering.

## Files

| File | Role |
|------|------|
| `loadImage.java` | `main` — load PNG, downscale, match, pack, write artifacts |
| `legoRender.java` | Procedural stud / packed-plate PNGs (no file I/O) |
| `pieceCount.java` | Format color tallies into a shopping-list report |
| `resources/` | **Input** PNGs only (e.g. `Jarvis.png`) |

Outputs are written under [`../artifacts/`](../artifacts/), not into `resources/`.

## `loadImage` flow

```text
Image/resources/<input>.png
        │
        ▼
  MergePixels(BLOCK_SIZE)   average each block
        │
        ▼
  ReScale                   one pixel per block (stud grid image)
        │
        ▼
  colorMatch.matchImage     → studs[][], colorCounts, flat mosaic image
        │
        ├─► pieceCount → artifacts/color_counts.txt
        │
        ├─► Pack (six modes) → bom_*.txt + bom_compare.txt
        │
        ├─► artifacts/output.png              (flat matched mosaic)
        ├─► legoRender.renderStuds            → output_lego.png
        └─► legoRender.renderPacked(each mode) → output_lego_{greedy,ilp,rle,component,dlx,anneal}.png
```

### Important constants (edit in `loadImage.java`)

| Constant | Meaning |
|----------|---------|
| `BLOCK_SIZE` | Pixels per stud when downscaling (larger → fewer studs) |
| `INPUT_IMAGE` | Path under `resources/` |
| `ARTIFACTS_DIR` | Output folder (`artifacts`) |
| `DB_PATH` | SQLite DB for colors + plate availability |

Comment out the packing block or individual render lines to skip those steps.

## `legoRender`

Pure functions — caller writes files.

| Method | Draws |
|--------|--------|
| `renderStuds(BufferedImage, studSizePx)` | Every stud as its own 1×1 visual plate + knob |
| `renderPacked(List<PlacedPart>, studs, studSizePx)` | Multi-stud plates as one continuous body + knobs per stud |
| `renderPacked(PackResult, studs, studSizePx)` | Same, from a pack result |

Colors for packed plates are sampled from `studs[y][x]` at each part’s origin. Default `DEFAULT_STUD_SIZE_PX = 24`.

## `pieceCount`

Does **not** re-scan the grid. Formats maps already filled during `matchImage`:

- total studs (= 1×1 piece count before packing)
- per-color lines sorted by count

## Run

From project root:

```bash
make run
```

Classpath includes `Color`, `Image`, and `Pack` (see root [`Makefile`](../Makefile)).

## Related docs

- [`../Color/README.md`](../Color/README.md) — matching
- [`../Pack/README.md`](../Pack/README.md) — packing overview + research · [`../Pack/ALGORITHM_WALKTHROUGH.md`](../Pack/ALGORITHM_WALKTHROUGH.md) — algorithm details
- [`../artifacts/README.md`](../artifacts/README.md) — output file list

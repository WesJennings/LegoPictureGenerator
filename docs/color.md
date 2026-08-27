# Color

Maps image pixels to real LEGO **elements** (part + color) using the Rebrickable SQLite database.

See also: [color/MATH.md](color/MATH.md).

## Files

| File | Role |
|------|------|
| `catalog.cpp` | Load palette from SQLite |
| `color_matcher.cpp` | Euclidean nearest-color match |

## What it does

1. **`loadCatalog(dbPath)`** — loads opaque `(part_num, color_id)` rows joined to `colors` (RGB + name) for part **`3024`** (Plate 1×1). Skips transparent colors and `color_id < 0`.
2. **`nearestIndex(argb, palette)`** — Euclidean RGB distance to pick the closest element.
3. **`matchImage(...)`** — for each pixel of the stud mosaic:
   - writes the matched RGB back into a buffer
   - fills a `StudGrid` of `LegoElement` (same dimensions)
   - tallies `colorId → count` and one sample element per color

The stud grid is what the [packers](packing.md) and packed renders consume. The flat image is what `renderStuds` uses.

## Types

- **`LegoElement`** — concrete part+color (`partNum`, `colorId`, `colorName`, `rgbHex`, `toArgb()`).

## Location & dependencies

- `native/src/catalog.cpp`, `native/src/color_matcher.cpp`
- SQLite 3 (`libsqlite3-dev`), parameterized queries, DB opened read-only
- Database at [`data/bricks.db`](../data/) (tables `elements`, `colors`), loaded
  once at startup
- Covered by `lego_host_tests`, which runs the matcher against an in-test
  SQLite fixture

## Notes

- Matching is to **elements that exist for the part**, not bare color names — so you only get colors that were actually produced for that plate.
- `is_trans` in dumps may be `t`/`f` or `True`/`False`; both are handled.

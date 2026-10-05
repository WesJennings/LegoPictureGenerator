# Color

Maps image pixels to real LEGO **elements** (part + color) using the Rebrickable SQLite database.

See also: [color/MATH.md](color/MATH.md).

## Files

| File | Role |
|------|------|
| `host/catalog.cpp` | Load palette from SQLite |
| `engine/color_matcher.cpp` | Nearest-color match (Euclidean OKLab) |

## What it does

1. **`loadCatalog(dbPath)`** — loads opaque `(part_num, color_id)` rows joined to `colors` (RGB + name) for part **`3024`** (Plate 1×1). Skips transparent colors and `color_id < 0`.
2. **`nearestIndex(argb, palette)`** — convert the stud to OKLab, pick the closest palette slot by Euclidean distance (palette OKLab is precomputed). The slot’s **sRGB** is what gets written out.
3. **`matchImage(...)`** — for each stud cell, writes matched ARGB and a palette index (`MatchResult`). It does **not** build the stud grid or tallies by itself.
4. **`studGridFromMatch` (in `pipeline.cpp`)** — maps palette indices to `LegoElement`s, builds the `StudGrid` packers read, and tallies `colorId → count` (plus one sample element per color) for `color-counts.txt` / optional stud BOM.

Matched ARGB feeds `matched.png`, `renderStuds`, and `renderPacked`. The stud grid is what the [packers](packing.md) consume.

## Types

- **`LegoElement`** — concrete part+color (`partNum`, `colorId`, `colorName`, `rgbHex`, `toArgb()`).

## Location & dependencies

- `native/src/host/catalog.cpp`, `native/src/engine/color_matcher.cpp`
- SQLite 3 (`libsqlite3-dev`), parameterized queries, DB opened read-only
- Database at [`data/bricks.db`](../data/) (tables `elements`, `colors`), loaded
  once at startup
- Covered by `lego_host_tests`, which runs the matcher against an in-test
  SQLite fixture

## Notes

- Matching is to **elements that exist for the part**, not bare color names — so you only get colors that were actually produced for that plate.
- Distance is Euclidean in **OKLab**, not sRGB. See [color/MATH.md](color/MATH.md).
- `is_trans` in dumps may be `t`/`f` or `True`/`False`; both are handled.

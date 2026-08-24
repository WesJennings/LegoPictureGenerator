# Color

Maps image pixels to real LEGO **elements** (part + color) using the Rebrickable SQLite database.

See also: [color/MATH.md](color/MATH.md).

## Files

| File | Role |
|------|------|
| `ColorMatcher.java` | Load palette from DB, nearest-color match, fill stud grid + optional tallies |

## What it does

1. **`loadElements(dbPath, partNum)`** — loads opaque `(part_num, color_id)` rows joined to `colors` (RGB + name). Default part is **`3024`** (Plate 1×1). Skips transparent colors and `color_id < 0`.
2. **`nearest(argb, palette)`** — Euclidean RGB distance to pick the closest element.
3. **`matchImage(...)`** — for each pixel of the stud mosaic:
   - writes the matched RGB back into the image
   - fills `LegoElement[][] studs` (same dimensions)
   - optionally tallies `colorId → count` and one sample element per color

The stud grid is what the [packers](packing.md) and packed renders consume. The flat image is what [`LegoRenderer.renderStuds`](image.md) uses.

## Types

- **`LegoColor`** — palette row from `colors` (`id`, `name`, `rgbHex`, `r/g/b`, `isTrans`).
- **`LegoElement`** — concrete part+color (`partNum`, `colorId`, `colorName`, `rgbHex`, `toArgb()`).

## Location & dependencies

- Class: `backend/src/main/java/com/legopicturegenerator/core/color/ColorMatcher.java`
- SQLite JDBC driver comes from Maven (`org.xerial:sqlite-jdbc`)
- Database at [`data/bricks.db`](../data/) (tables `elements`, `colors`), loaded
  once at startup by `CatalogProvider`
- Covered by `PipelineServiceTest`, which runs the matcher against an in-test
  SQLite fixture

## Notes

- Matching is to **elements that exist for the part**, not bare color names — so you only get colors that were actually produced for that plate.
- `is_trans` in dumps may be `t`/`f` or `True`/`False`; both are handled.

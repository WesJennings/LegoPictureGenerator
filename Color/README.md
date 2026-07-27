# Color

Maps image pixels to real LEGO **elements** (part + color) using the Rebrickable SQLite database.

## Files

| File | Role |
|------|------|
| `colorMatch.java` | Load palette from DB, nearest-color match, fill stud grid + optional tallies |

## What it does

1. **`loadElements(dbPath, partNum)`** — loads opaque `(part_num, color_id)` rows joined to `colors` (RGB + name). Default part is **`3024`** (Plate 1×1). Skips transparent colors and `color_id < 0`.
2. **`nearest(argb, palette)`** — Euclidean RGB distance to pick the closest element.
3. **`matchImage(...)`** — for each pixel of the stud mosaic:
   - writes the matched RGB back into the image
   - fills `LegoElement[][] studs` (same dimensions)
   - optionally tallies `colorId → count` and one sample element per color

The stud grid is what [`Pack/`](../Pack/) and packed renders consume. The flat image is what [`legoRender.renderStuds`](../Image/README.md) uses.

## Types

- **`LegoColor`** — palette row from `colors` (`id`, `name`, `rgbHex`, `r/g/b`, `isTrans`).
- **`LegoElement`** — concrete part+color (`partNum`, `colorId`, `colorName`, `rgbHex`, `toArgb()`).

## Standalone test

```bash
# from project root, after compile
java -cp "lib/sqlite-jdbc-3.53.2.0.jar:Color" colorMatch
```

Prints how many opaque `3024` elements loaded and the first 10.

## Dependencies

- JDBC SQLite via [`lib/`](../lib/)
- Database at [`data/bricks.db`](../data/) (tables `elements`, `colors`)

## Notes

- Matching is to **elements that exist for the part**, not bare color names — so you only get colors that were actually produced for that plate.
- `is_trans` in dumps may be `t`/`f` or `True`/`False`; both are handled.

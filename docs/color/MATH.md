# Color matching math

**Goal:** map each stud’s sRGB to the nearest real LEGO element color in the
palette (opaque plates that exist for part `3024` by default).

Formulas use plain text / Unicode for normal Markdown preview.

## Space

Matching is **not** done in sRGB. Each color is converted to [OKLab](https://bottosson.github.io/posts/oklab/)
(Björn Ottosson): linearize sRGB, 3×3 to LMS, cube-root, then 3×3 to `L a b`.
The brick palette is converted **once** at catalog load (`makePaletteEntry` /
`paletteRgb`). Each stud is converted in `nearestIndex`.

The winner is still a palette **index**. Output pixels use that brick’s original
sRGB — there is no conversion back from OKLab.

## Distance

For a stud `q` and palette entry `i` in OKLab:

```text
di² = (Lq − Li)² + (aq − ai)² + (bq − bi)²
```

Choose the palette index with the smallest `di²`. Ties keep the first minimum
found while scanning the palette.

**Code:** `lego::srgbToOklab`, `lego::nearestIndex` (`native/src/engine/color_matcher.cpp`).

**Assumptions:** OKLab Euclidean is a perceptual approximation, not CIEDE2000.
Transparent and unknown colors are excluded when the palette is loaded.
sRGB is treated as the usual 2.4-gamma encoding (IEC 61966-2-1).

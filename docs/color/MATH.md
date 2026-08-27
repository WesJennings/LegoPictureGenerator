# Color matching math

**Goal:** map each stud’s RGB to the nearest real LEGO element color in the
palette (opaque plates that exist for part `3024` by default).

Formulas use plain text / Unicode for normal Markdown preview.

## Distance

For a stud color `(r, g, b)` and palette entry `(ri, gi, bi)`:

```text
di² = (r − ri)² + (g − gi)² + (b − bi)²
```

Choose the palette index with the smallest `di²` (squared Euclidean RGB). Ties
keep the first minimum found while scanning the palette.

**Code:** `ColorMatcher.nearest` → C++ `lego::nearestIndex` (`native/src/color_matcher.cpp`).

**Assumptions:** matching is in sRGB channel space, not a perceptual
uniform space (CIELAB / redmean). Transparent and unknown colors are excluded
when the palette is loaded.

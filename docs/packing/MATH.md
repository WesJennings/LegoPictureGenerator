# Packing math

**Goal:** cover every stud of a color-matched grid with catalog plates so each
stud is covered **exactly once**, each plate is a solid color present in the
DB, and the number of plates (pieces) is minimized.

Formulas use plain text / Unicode for normal Markdown preview.

## Cardinality

Let `S` be the set of studs and `Plates` a set of placed plates. Feasibility:

- union of all `cells(p)` for `p` in `Plates` equals `S`
- plates are pairwise cell-disjoint
- each plate footprint is a catalog size for that color

Objective (min-cardinality):

```text
minimize  |Plates|
```

There is no simple formula mapping stud count to optimal `|Plates|`; it
depends on connected same-color regions and available plate sizes.

## DLX / Algorithm X (sketch)

`DlxPacker` solves exact cover on each same-color connected component (blob):

1. Flood-fill a mono-color component of `n` cells.
2. Enumerate legal plate placements that lie entirely inside the component.
3. Search an exact cover of the `n` cells (Algorithm X style), preferring
   columns with the fewest remaining options (DLX heuristic).
4. Small components use bitmask DP (`n ≤ 64`); larger / slow components
   fall back to greedy within time budgets (~30s per component, ~60s pack).

Status strings: `optimal` if every blob was exact; otherwise
`exact_partial (... greedy fallback)`.

**Code:** `DlxPacker` → C++ `lego::packDlx` (`native/src/packers.cpp`). Full narrative: [algorithm-walkthrough.md](../algorithm-walkthrough.md).

## Why piece targets need search

Because `P* = min |Plates|` is combinatorial, “aim for `N` pieces”
adjusts the **input grid size** and re-packs (see [sizing/MATH.md](../sizing/MATH.md))
instead of inverting a formula.

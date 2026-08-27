# Packing algorithms — visual walkthrough

ASCII diagrams only (no Mermaid) so they show correctly in the editor.

The six algorithms below are implemented in C++ (`native/src/packers.cpp`) with
the same control flow as the original Java. Java packer classes are JNI
wrappers. See [native.md](native.md).

**Docs split:** algorithm explanations live **here**. High-level types, wiring, BOM, and research papers live in [`README.md`](packing.md). Formulas: [`packing/MATH.md`](packing/MATH.md).

**Numbering:** Setup A–B first, then **algorithms 1–6** (one number per packer), then compare / outputs / cheat sheet.

## Table of contents

- [Setup A — Pipeline](#setup-a--pipeline)
- [Setup B — Shared toy example](#setup-b--shared-toy-example)
- [1. Greedy (`greedy`)](#1-greedy-greedy)
- [2. ILP (`ilp`)](#2-ilp-ilp)
- [3. RLE (`rle`)](#3-rle-rle)
- [4. Component greedy (`component`)](#4-component-greedy-component)
- [5. DLX (`dlx`)](#5-dlx-dlx)
- [6. Anneal (`anneal`)](#6-anneal-anneal)
- [7. Side-by-side comparison](#7-side-by-side-comparison)
- [8. How results are used after packing](#8-how-results-are-used-after-packing)
- [9. Cheat sheet](#9-cheat-sheet)

---

## Setup A — Pipeline

```text
  Input PNG
      │
      ▼
  Downscale to studs
      │
      ▼
  ColorMatcher  ──────────────────────►  flat mosaic PNG
      │                                 (output_lego.png)
      ▼
  studs[][]  (READ ONLY — never rewritten)
      │
      ├──────────┬──────────┬──────────┬──────────┬──────────┐
      ▼          ▼          ▼          ▼          ▼          ▼
   greedy      ilp        rle     component     dlx      anneal
  GreedyPack ExactIlp   RlePack  ComponentG  DlxPack  AnnealPack
      │          │          │          │          │          │
      └──────────┴──────────┴────┬─────┴──────────┴──────────┘
                                 ▼
                    PackResult (List<PlacedPart>)
                                 │
                                 ▼
              BOM + PackCompare + packed LEGO PNGs
              (bom-<mode>.txt, lego-<mode>.png per job)
```

```text
studs (unchanged)          placed list (output)
┌─────────────┐            ┌──────────────────────┐
│ B B B B R R │            │ 2×2 Black @ (0,0)     │
│ B B B B R · │  ──pack──► │ 2×2 Black @ (2,0)     │
│ · · · · · · │            │ 1×2 Red   @ (4,0)     │
└─────────────┘            │ 1×1 Red   @ (4,1)     │
                           └──────────────────────┘
```

---

## Setup B — Shared toy example

```text
     x0  x1  x2  x3  x4  x5
  y0  B   B   B   B   R   R
  y1  B   B   B   B   R   ·
  y2  ·   ·   ·   ·   ·   ·

  B = Black   R = Red   · = empty
```

Catalog (simplified): both colors have `2×2`, `1×2`, `1×1` (plus larger sizes that won’t fit here).

---

## 1. Greedy (`greedy`)

### Big picture

```text
                         pack()
                            │
        ┌───────────┬───────┼───────┬───────────┐
        ▼           ▼       ▼       ▼           │
   order L→R   order R→L  bottom↑  col-major    │
   top→bot     top→bot    L→R                   │
        │           │       │       │           │
        ▼           ▼       ▼       ▼           │
     repair      repair  repair  repair         │
        │           │       │       │           │
        └───────────┴───────┼───────┘           │
                            ▼                   │
                   keep fewest pieces           │
                            │                   │
                            ▼                   │
                  PackResult mode="greedy"  ◄─────┘
```

At **each uncovered cell**: try sizes **largest → smallest** until one fits; mark those cells in `covered`.

---

### Progress on the toy grid

### Step G0 — start

```text
covered: all false
placed:  []

Grid:  B B B B R R
       B B B B R ·
```

### Step G1 — at (0,0) Black

```text
  Try sizes large → small:
       2×2 fits?  YES
          │
          ▼
  Place 2×2 @ (0,0)
  Mark 4 cells covered

  [====2×2====] B B R R
  [===========] B B R ·

  placed:  3022 2×2 Black @ (0,0)
```

### Step G2 — skip already-covered cells

```text
  Visit (1,0), (0,1), (1,1)... already covered → do nothing
  Next free Black cell = (2,0)
```

### Step G3 — at (2,0) Black

```text
  [====2×2====][====2×2====] R R
  [===========][===========] R ·

  placed:
    3022 2×2 Black @ (0,0)
    3022 2×2 Black @ (2,0)
```

### Step G4 — at (4,0) Red

```text
  [====2×2====][====2×2====][1×2R]
  [===========][===========] R ·

  placed: … + 3023 1×2 Red @ (4,0)
```

### Step G5 — at (4,1) Red

```text
  [====2×2====][====2×2====][1×2R]
  [===========][===========][1]

  placed: … + 3024 1×1 Red @ (4,1)
```

### Step G6 — main pass done

```text
  loop:
    for each cell in scan order:
      if uncovered:
        place largest plate that fits
        mark covered
  → every stud is already covered
  → then run repair (merge leftover 1×1s if any)
```

Repair is **not** for missing holes — the main pass already covers everything. Repair tries to **merge 1×1 debris** into larger plates.

---

### Repair

Awkward leftover example (same color strip):

```text
Main pass left four 1×1s:

  [#][#][#][#]     four separate 3024s
```

```text
  For each placement from main pass:
      if size is 1×1  →  mark cell as "need re-pack"
      else            →  keep it, leave covered

  Then scan need-cells from bottom-right → top-left
      and call placeAt again (may place a 1×4, etc.)

  Finally: safety sweep for any uncovered studs
```

```text
After repair (ideal):

  [====== 1×4 ======]     one piece instead of four
```

Then greedy **compares** the 4 scan-order results and keeps the fewest pieces.

---

## 2. ILP (`ilp`)

### Big picture

```text
  pack()
    │
    ▼
  scan every stud ──────────────────────┐
    │                                   │
    │  if visited or empty → skip       │
    ▼                                   │
  BFS flood → same-color blob           │
    │                                   │
    ▼                                   │
  cells ≤ 64 AND time left?             │
    │              │                    │
   yes             no                   │
    │              │                    │
    ▼              ▼                    │
  enumerate     greedyComponent         │
  placements       │                    │
  as bitmasks      │                    │
    │              │                    │
    ▼              │                    │
  branch-and-bound │                    │
  (min pieces,     │                    │
   exact cover)    │                    │
    │              │                    │
    │ timeout /    │                    │
    │ no solution?─┘                    │
    │                                   │
    ▼                                   │
  append PlacedParts for this blob      │
    │                                   │
    └───────────────────────────────────┘
                    │
                    ▼
         PackResult mode="ilp"
```

**BFS** finds blobs. **ILP-style search** solves each small blob. Large/timeout blobs use **greedy**.

---

### BFS makes color islands

```text
  Same grid:

    B B B B R R
    B B B B R ·

  4-neighbor flood (up/down/left/right), same color only:

  Blob Black (8 cells)          Blob Red (3 cells)
  ┌─────────────┐               ┌───────┐
  │ B B B B · · │               │ ··· R R │
  │ B B B B · · │               │ ··· R · │
  └─────────────┘               └───────┘
```

Each blob is packed **independently**, then lists are concatenated.

---

### Enumerate placements → bitmasks

Red blob — assign bit indices:

```text
  cell (4,0) → bit 0
  cell (5,0) → bit 1
  cell (4,1) → bit 2

  full cover = bits 0,1,2 all on  →  mask 111
```

Legal placements become subsets:

```text
Placement                     mask (bit2 bit1 bit0)
────────────────────────────  ────────────────────
1×2 @ (4,0) covers cells 0,1      0    1    1
1×1 @ (4,0)                       0    0    1
1×1 @ (5,0)                       0    1    0
1×1 @ (4,1)                       1    0    0
1×2 vertical @ (4,0) if fits      1    0    1
```

How the pool is built:

```text
  blob cells
      │
      ▼
  map each cell → bit index
      │
      ▼
  for each cell as top-left:
    for each plate size:
      if whole footprint is inside this blob
         → add Placement + bitmask
      else
         → reject
      │
      ▼
  always also add a 1×1 for every cell
      │
      ▼
  pool of candidate "columns" for the ILP
```

---

### Branch-and-bound progress (Red blob)

Goal: pick masks that **don’t overlap** and together equal `111`, using **as few** placements as possible.

```text
                         covered=000  pieces=0
                                  │
                     must cover lowest free bit (0)
                         ┌────────┴────────┐
                         ▼                 ▼
              try 1×2 (mask 011)    try 1×1 (mask 001)
                         │                 │
                         ▼                 ▼
              covered=011 pieces=1   covered=001 pieces=1
                         │                 │
              must cover bit 2       … more 1×1s …
                         │                 │
                         ▼                 ▼
              add 1×1 (mask 100)     covered=111 pieces=3
                         │                 (worse)
                         ▼
              covered=111 pieces=2  ★ best
```

Progress as bits fill in:

```text
  covered bits:   _ _ _     →    # # _     →    # # #
  pieces:           0              1              2
  choice:           —         1×2 @ (4,0)    1×1 @ (4,1)
```

Pruning:

- Best so far is **2** pieces → drop any branch that already has **2** before the blob is full.
- Lower bound: 5 cells left, biggest piece size 2 → need ≥ 3 more; if that can’t beat best → cut.

---

### Black blob (short)

```text
  8 Black cells → many placements (2×2, 2×4, 1×4, …)

  Good:   2×2 @ (0,0) + 2×2 @ (2,0)   →  2 pieces
  Bad:    eight 1×1s                  →  8 pieces
```

If a blob has **> 64** cells, exact search is skipped → `greedyComponent` (status `exact_partial`).

---

## 3. RLE (`rle`)

### Big picture

Strip-first packing (`RlePacker`): cover each row as horizontal runs, then stack matching strips into taller plates.

```text
  pack()
    │
    ▼
  Phase A — for each row y:
      find maximal same-color runs
      split each run into 1-high catalog strips
      (largest 1×N that fits, then leftovers)
    │
    ▼
  Phase B — vertical merge:
      while two strips share x, width, color
      and the lower one sits exactly under the upper
      and catalog has (w × combined height):
         replace both with one taller plate
    │
    ▼
  PackResult mode="rle"
```

---

### Progress on the toy grid

### Step R0 — Phase A, row 0

```text
  Run Black cols 0–3 length 4 → place 1×? strips
  Prefer largest 1-high: e.g. one 1×4 if in catalog,
  else 1×2 + 1×2, etc.

  Example with 1×2 available:
    [1×2][1×2][1×2 R]
    B B B B R ·     ← row 1 not packed yet
```

(If `1×4` exists for Black, row 0 Black becomes one strip.)

### Step R1 — Phase A, row 1

```text
  Same Black run under row 0 → more 1-high strips
  Red at (4,1) → 1×1

  After Phase A (illustrative with 1×4 Black):
    [====1×4====][1×2R]
    [====1×4====][1]
```

### Step R2 — Phase B vertical merge

```text
  Upper: 1×4 Black @ (0,0)
  Lower: 1×4 Black @ (0,1)   same x, w, color, adjacent rows
  Catalog has 2×4?  YES
       │
       ▼
  Replace both with 2×4 Black @ (0,0)

  [========2×4========][1×2R]
  [===================][1]
```

Red strips don’t stack (only one cell under the 1×2).

**vs greedy:** RLE never tries a `2×2` during Phase A — only 1-high first — so odd 2D shapes often need more pieces until merge helps.

---

## 4. Component greedy (`component`)

### Big picture

Same largest-first rule as greedy, but scoped to each color island (`ComponentGreedyPacker`). No multi-order scan, no 1×1 repair.

```text
  pack()
    │
    ▼
  scan grid; for each unvisited non-null stud:
      BFS flood → blob (same color, 4-neighbor)
      mark outside blob as already "covered"
      largest-first greedy inside blob only
    │
    ▼
  PackResult mode="component"
```

---

### Progress on the toy grid

```text
  Blob Black (8 cells) → greedy inside:
      at (0,0): 2×2 fits → place
      at (2,0): 2×2 fits → place
      → 2 pieces

  Blob Red (3 cells) → greedy inside:
      at (4,0): 1×2 fits → place
      at (4,1): 1×1
      → 2 pieces

  Total: same shape as a good greedy pass on this tiny grid
```

**vs greedy (`greedy` mode):** on real photos, `component` skips the 4 scan-orders + repair, so it can match greedy piece count (as on Jarvis) or do slightly worse — useful ablation of “is repair helping?”

---

## 5. DLX (`dlx`)

### Big picture

Same problem as `ilp` (min placements, exact cover per blob). Different **branching rule** (`DlxPacker`).

```text
  Same outer loop as ILP:
      BFS blobs → size/time check → enumerate bitmasks

  Search difference:
      ILP:  always cover the lowest-index uncovered bit
      DLX:  always cover the uncovered bit with the FEWEST
            remaining legal placements  ← Algorithm X heuristic

  Then try each placement that covers that bit (no overlap),
  recurse, minimize piece count, prune like ILP.
```

```text
  Why “fewest options” is faster:
      hard cells (only 1–2 plates fit) get decided early
      → search tree stays thinner → more blobs finish
        before the time budget → fewer greedy fallbacks
```

---

### Progress on the Red blob

Same bitmasks as [§2 ILP](#2-ilp-ilp) (enumerate placements). At the start every cell has several covers; after placing a big piece, counts change.

```text
  uncovered = bits 0,1,2

  Count legal placements covering each bit (illustrative):
      bit 0: 3 options
      bit 1: 3 options
      bit 2: 2 options   ← fewest → branch on bit 2 first

  Try placements that include bit 2 (e.g. 1×1 @ (4,1), or vertical 1×2):
      …
  Same goal as ILP: covered == 111 with min pieces
```

On Jarvis-scale runs, DLX often ties or beats `ilp` on pieces and finishes sooner for the same budgets.

---

## 6. Anneal (`anneal`)

### Big picture

Metaheuristic polish (`AnnealPacker`): start from a full greedy solution, then randomly re-pack windows.

```text
  pack()
    │
    ▼
  seed = GreedyPacker.pack(studs)     // full multi-order greedy
  best = current = seed
  T = temperature
    │
    ▼
  loop (until time ~20s or max iters):
      pick random window (e.g. 8×8)
      drop every PlacedPart that intersects the window
      re-greedy the holes with SHUFFLED catalog order
      Δ = newPieceCount - currentPieceCount
      if Δ ≤ 0:           accept
      else if rand < e^(-Δ/T): accept anyway
      if accepted and better than best: keep best
      cool T *= 0.995
    │
    ▼
  PackResult mode="anneal"
  status = "improved" if best < seed, else "ok"
```

---

### One move (sketch)

```text
  Before (window over left Blacks):
    [2×2][2×2][1×2R]
    [    ][    ][1]

  Clear parts touching window → holes on left
  Re-pack with shuffled sizes (maybe tries 1×2s first):
    could get worse, same, or (rarely) better locally

  Accept if fewer pieces, or probabilistically if more
  while T is still high
```

**vs greedy:** needs the seed to leave room for improvement; on some mosaics status stays `"ok"` (no better than seed within the budget).

---

## 7. Side-by-side comparison

```text
                         studs[][]
                             │
     ┌───────┬───────┬───────┼───────┬───────┐
     ▼       ▼       ▼       ▼       ▼       ▼
  greedy    ilp     rle  component  dlx   anneal
  whole   blobs+  strips  blobs+   blobs+  SA on
  grid+   B&B     +merge  greedy   X-heur  greedy
  repair  (low    1-high  no       fewest  seed
          bit)            repair   opts
```

| # | Mode | Scope | Core decision | Typical role |
|---|------|--------|---------------|--------------|
| 1 | greedy | Full grid | Largest-first + multi-order repair | Default heuristic |
| 2 | ilp | Per blob | Min cover, lowest-bit branch | Exact-style baseline |
| 3 | rle | Rows then stack | Strip split + vertical merge | Strip ablation |
| 4 | component | Per blob | Largest-first only | Ablation vs greedy repair |
| 5 | dlx | Per blob | Min cover, fewest-options branch | Usually best exact-style |
| 6 | anneal | Full grid | Local window SA | Try to beat greedy |

---

## 8. How results are used after packing

Each mode returns a `PackResult` → BOM text + packed PNG. Wiring, status strings, and file roles live in [`README.md`](packing.md) (high-level); this walkthrough stops at *how* each packer builds the placement list.

| Artifact | Source |
|----------|--------|
| `bom_{mode}.txt` | Per-mode shopping list |
| `bom_compare.txt` | Piece count / time / deltas vs greedy |
| `output_lego_{mode}.png` | Packed render |
| `output_lego.png` | Flat 1×1 mosaic (no packing) |

---

## 9. Cheat sheet

```text
1 GREEDY
  for each scan order:
    for each cell:
      if free: place largest plate that fits → mark covered
    repair: re-pack leftover 1×1s from opposite corner
  keep schedule with fewest pieces

2 ILP
  for each same-color BFS blob:
    if too big or no time: greedy that blob
    else:
      build all legal placements as bitmasks
      branch-and-bound on lowest uncovered bit, min pieces
  concatenate all blob placements

3 RLE
  per row: maximal same-color runs → split into 1-high catalog strips
  vertically merge stacked same-span strips when taller catalog part exists

4 COMPONENT
  BFS same-color blobs → largest-first greedy per blob (no multi-order repair)

5 DLX
  same blob split as ilp; Algorithm X with fewest-options column pick
  minimize placement count; greedy fallback on large/timeout blobs

6 ANNEAL
  seed from greedy; randomly clear a window; re-pack with shuffled sizes
  accept better (or worse with exp(-Δ/T)); keep best within time budget
```

```text
                    PACKING
                       │
       ┌───────┬───────┼───────┬───────┬───────┐
       ▼       ▼       ▼       ▼       ▼       ▼
    INPUT  greedy   ilp    rle  component dlx/anneal
  studs R/O  …       …      …      …        …
                       │
                       ▼
                    OUTPUT
              PlacedPart list + BOM + packed render
```

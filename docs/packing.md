# Pack — LEGO Plate Packing

Packing turns a **stud grid of matched LEGO colors** into a **list of physical plates** that cover every stud exactly once. Goal: fewer pieces to buy, using only parts that exist in that color in `data/bricks.db`.

The stud grid is **never modified**. Packers only **read** `studs` and emit `PlacedPart` lists. Implementations are C++ (`native/src/packers.cpp`) with the same control flow as the original Java. See [native.md](native.md).

**Docs split:** this README is the high-level map (types, wiring, BOM, research). Step-by-step algorithm explanations live only in [`algorithm-walkthrough.md`](algorithm-walkthrough.md). Formulas: [`packing/MATH.md`](packing/MATH.md).

**Related:** [root](../README.md) · [Color](color.md) · [Image](image.md) · [Architecture](architecture.md) · [MATH index](MATH.md)

---

## Table of contents

1. [High-level overview](#1-high-level-overview)
2. [How files work together](#2-how-files-work-together)
3. [Coordinate system](#3-coordinate-system)
4. [Shared types](#4-shared-types)
5. [`PlateCatalog`](#5-platecatalog)
6. [Algorithms → walkthrough](#6-algorithms--walkthrough)
7. [BOM, wiring, status](#7-bom-wiring-status)
8. [Research papers & further reading](#8-research-papers--further-reading)

---

## 1. High-level overview

After color matching, every cell is one LEGO color. Packing asks:

> Cover every stud with catalog plates so that each stud is covered **exactly once**, every plate is a **single solid color**, the part+color exists in the DB, and we prefer **fewer pieces**.

| # | Mode | Class | One-line strategy |
|---|------|--------|-------------------|
| **1** | `greedy` | `GreedyPacker` | Largest-first + multi scan-order + 1×1 repair |
| **2** | `ilp` | `ExactIlpPacker` | Per-blob B&B set-partition; large blobs → greedy |
| **3** | `rle` | `RlePacker` | Row RLE strips + vertical merge |
| **4** | `component` | `ComponentGreedyPacker` | Per-blob largest-first (no repair) |
| **5** | `dlx` | `DlxPacker` | Same model as ILP; fewest-options Algorithm X search |
| **6** | `anneal` | `AnnealPacker` | SA local search from a greedy seed |

```text
studs[][] ──► PlateCatalog ──► packers 1–6 ──► PackResult
                                      │
                                      ▼
                         formatBom / placements JSON / packed PNGs
```

**Not packing’s job:** rewriting `studs`, drawing the flat mosaic (`matched.png`) or stud preview (`lego-studs.png`), or producing a left-to-right build order (list order is “placement order”).

---

## 2. How files work together

| File | Role |
|------|------|
| `catalog.cpp` (`PlateCatalog`) | Allowed sizes + DB color filter; footprints largest-first |
| `types.hpp` (`PlacedPart` / `PackResult`) | One plate / one algorithm run |
| `packers.cpp` | Modes 1–6 (see walkthrough) |
| `text.cpp` | Shopping-list BOM formatting |

Algorithms depend on catalog + shared types; BOM only needs `PackResult`. Packers do not write images (`renderer.cpp` does that).

---

## 3. Coordinate system

```text
studs[y][x]   ← row y (top = 0), column x (left = 0)

PlacedPart (x, y, w, h) covers [x .. x+w-1] × [y .. y+h-1]
```

Example: `1×4` at `(0,0)` covers `(0,0)…(3,0)`; the next plate beside it starts at `(4,0)`.

Packers allocate their own `covered[][]` / `visited[][]`. **`studs` stays read-only.**

---

## 4. Shared types

**`PlacedPart`:** `partNum`, `colorId`, `colorName`, top-left `(x,y)`, footprint `(w,h)`.

**`PackResult`:** `modeName`, `placed`, `elapsedMs`, `status` (`ok` / `optimal` / `exact_partial (...)` / `improved`). `pieceCount()` = `placed.size()`.

---

## 5. `PlateCatalog`

Hardcoded `BASE` sizes (edit to change what packers may use — keep `3024` 1×1) plus rotations (`w≠h` → both orientations), sorted **largest area first**. `footprintsForColor(colorId)` keeps only parts that exist in that color in `bricks.db`.

To add a size: put it in `BASE`, ensure it exists in `elements` for the colors you care about. To forbid a size: remove it from `BASE`.

---

## 6. Algorithms → walkthrough

**All algorithm explanations are in [`algorithm-walkthrough.md`](algorithm-walkthrough.md)** (same numbering 1–6), including ASCII progress on a shared toy grid, side-by-side compare, and a cheat sheet.

| # | Walkthrough section |
|---|---------------------|
| 1 | [Greedy](algorithm-walkthrough.md#1-greedy-greedy) |
| 2 | [ILP](algorithm-walkthrough.md#2-ilp-ilp) |
| 3 | [RLE](algorithm-walkthrough.md#3-rle-rle) |
| 4 | [Component](algorithm-walkthrough.md#4-component-greedy-component) |
| 5 | [DLX](algorithm-walkthrough.md#5-dlx-dlx) |
| 6 | [Anneal](algorithm-walkthrough.md#6-anneal-anneal) |

Do not duplicate those walkthroughs here.

---

## 7. BOM, wiring, status

**BOM** (`formatBom` in `text.cpp`): shopping counts by `(partNum, color)` — locations dropped. Side-by-side mode comparison is done in the web UI (`CompareResults`) when multiple modes run; there is no `bom-compare.txt` artifact.

**Wiring** (`native/src/pipeline.cpp`): `loadCatalog` once at startup → run the requested packers → write `bom-<mode>.txt`, `placements-<mode>.json`, and packed `lego-<mode>.png` into the job's output directory. The web UI defaults to `greedy`, can pick any single mode or compare-all (all six), and forces `dlx` when `sizing=pieces`; the CLI accepts any comma-separated mode list.

| Status | Meaning |
|--------|---------|
| `ok` | Finished normally (greedy / rle / component; anneal with no improvement) |
| `improved` | Anneal beat its greedy seed |
| `optimal` | ILP/DLX: every blob solved without greedy fallback |
| `exact_partial (...)` | Some blobs fell back (size > 64 and/or timeout) |

`ilp` / `dlx` use `EXACT_CELL_LIMIT = 64` (`uint64_t` bitmasks). Large same-color regions are hybrids, not a full-image MIP.

```text
docs/
  packing.md                 ← high-level + research (this file)
  algorithm-walkthrough.md   ← algorithm explanations
native/src/packers.cpp / native/include/lego/packers.hpp
native/src/catalog.cpp       ← PlateCatalog + BASE sizes
native/src/text.cpp          ← BOM formatting
```

---

## 8. Research papers & further reading

These papers are background for what this folder implements. None are required to run the code.

### Exact cover, set partition, ILP / B&B (`ExactIlpPacker`, shared with `DlxPacker`)

| Paper | Why it matters here | Link |
|-------|---------------------|------|
| Donald E. Knuth, *Dancing Links* (2000). arXiv:cs/0011047 | Exact cover / **Algorithm X**: subsets that cover each element exactly once — the model for covering studs with plate placements. | [arXiv abs](https://arxiv.org/abs/cs/0011047) · [PDF](https://arxiv.org/pdf/cs/0011047) |
| A. H. Land & A. G. Doig, *An Automatic Method of Solving Discrete Programming Problems*, Econometrica 28(3), 1960 | Foundational **branch-and-bound** for integer programs — prune when a partial solution cannot beat the best. | [PDF](https://jmvidal.cse.sc.edu/library/land60a.pdf) |
| Karla L. Hoffman & Manfred Padberg, *Set Covering, Packing and Partitioning Problems* (survey) | Set cover vs packing vs **partitioning** (`ilp`/`dlx` are set partitioning: each stud covered exactly once). | [PDF](http://seor.vse.gmu.edu/~khoffman/Set_covering_set_packing_set_partitioning.pdf) |

### Algorithm X / DLX heuristic (`DlxPacker`)

| Paper | Why it matters here | Link |
|-------|---------------------|------|
| Donald E. Knuth, *Dancing Links* (same as above) | **Fewest-options column choice** is the classic Algorithm X / DLX branching heuristic our `DlxPacker` uses (vs `ilp`’s lowest-bit order). Full dancing-links lists are optional; the heuristic is the important part. | [arXiv](https://arxiv.org/abs/cs/0011047) |
| Wikipedia: *Knuth’s Algorithm X* | Short readable summary of the matrix formulation and recursive search. | [Article](https://en.wikipedia.org/wiki/Knuth%27s_Algorithm_X) |

### Greedy / largest-first (`GreedyPacker`, `ComponentGreedyPacker`)

| Paper | Why it matters here | Link |
|-------|---------------------|------|
| B. S. Baker, E. G. Coffman Jr. & R. L. Rivest, *Orthogonal Packings in Two Dimensions*, SIAM J. Comput. 9(4), 1980 | Classic **greedy orthogonal rectangle packing** analysis — same family as largest-first plate placement. | [SIAM](https://epubs.siam.org/doi/10.1137/0209064) |

### Row RLE / strip decomposition (`RlePacker`)

| Paper / source | Why it matters here | Link |
|----------------|---------------------|------|
| Solomon W. Golomb, *Polyominoes* (book; polyomino & strip tiling ideas) | Classic background for covering regions with fixed shapes; strip/row thinking shows up throughout tiling literature. | [Publisher info](https://press.princeton.edu/books/paperback/9780691004730/polyominoes) |
| [wengraf/LEGOMosaics](https://github.com/wengraf/LEGOMosaics) | Practical mosaic pipeline that merges adjacent same-color groups after “legoizing” — close in spirit to row runs + merge. | [GitHub](https://github.com/wengraf/LEGOMosaics) |
| Run-length encoding (signal/image processing surveys) | Phase A is literally RLE on each row of a binary color mask before catalog snapping. | [Wikipedia: RLE](https://en.wikipedia.org/wiki/Run-length_encoding) |

### Connected components / flood fill (`ComponentGreedyPacker`, outer loops of `ilp`/`dlx`)

| Paper | Why it matters here | Link |
|-------|---------------------|------|
| A. Rosenfeld & J. L. Pfaltz, *Sequential Operations in Digital Picture Processing*, JACM 13(4), 1966 | Early formal treatment of **connected component** operations on digital images — the ancestor of BFS/DFS blob extraction on a grid. | [ACM](https://dl.acm.org/doi/10.1145/321356.321357) |
| Connected-component labeling (overview) | Modern summary of 4-/8-connectivity labeling used everywhere in vision pipelines. | [Wikipedia](https://en.wikipedia.org/wiki/Connected-component_labeling) |

### Simulated annealing (`AnnealPacker`)

| Paper | Why it matters here | Link |
|-------|---------------------|------|
| S. Kirkpatrick, C. D. Gelatt & M. P. Vecchi, *Optimization by Simulated Annealing*, Science 220(4598), 1983 | Foundational SA: accept worse moves with `exp(−Δ/T)`, cool `T` — the accept/cool loop in `AnnealPacker`. | [Science](https://www.science.org/doi/10.1126/science.220.4598.671) · [PDF](https://sci2s.ugr.es/sites/default/files/files/Teaching/GraduatesCourses/Metaheuristicas/Bibliography/1983-Science-Kirkpatrick-sim_anneal.pdf) |
| Kathryn A. Dowsland, *Some experiments with simulated annealing techniques for packing problems*, EJOR 68(3), 1993 | Early SA experiments specifically on **packing** layouts — same metaheuristic family as window re-pack moves. | [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/037722179390195S) · [DOI](https://doi.org/10.1016/0377-2217(93)90195-s) |

### LEGO construction / mosaic packing (domain)

| Paper | Why it matters here | Link |
|-------|---------------------|------|
| Torkil Kollsker & Enrico Malaguti, *Models and algorithms for optimising two-dimensional LEGO constructions*, EJOR 289(1), 2021 | 2D LEGO brick placement as OR: constructive **greedy** + **MILP** / matheuristics — closest published sibling. | [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S0377221720306159) · [IDEAS](https://ideas.repec.org/a/eee/ejores/v289y2021i1p270-284.html) |
| Torkil Kollsker, *Mathematical Models and Algorithms for Optimisation of the LEGO Construction Problem* (PhD, DTU) | Longer treatment: heuristics, MIP, large-neighborhood search. | [PDF](https://backend.orbit.dtu.dk/ws/portalfiles/portal/236623063/PhD_Thesis_Torkil_Kollsker.pdf) |
| Torkil Kollsker & Enrico Malaguti, *Optimisation and Static Equilibrium of Three-Dimensional LEGO Constructions*, OR Forum, 2021 | 3D extension (ALNS + MIP + stability). | [DOI](https://doi.org/10.1007/s43069-021-00062-3) |

### How this maps to our modes

```text
1 GreedyPacker (greedy)
  ≈ largest-first / multi-start orthogonal packing
    (Baker–Coffman–Rivest; LEGO constructive heuristics)

2 ExactIlpPacker (ilp)
  ≈ set-partition ILP per blob (Hoffman–Padberg)
    + branch-and-bound (Land–Doig), lowest-bit branching

3 RlePacker (rle)
  ≈ row run-length encoding + strip/vertical merge
    (RLE + polyomino/strip tiling practice; LEGOMosaics-style merges)

4 ComponentGreedyPacker (component)
  ≈ connected-component decomposition (Rosenfeld lineage)
    + same largest-first inner packer (ablation of repair)

5 DlxPacker (dlx)
  ≈ same exact-cover model (Knuth Algorithm X)
    + fewest-options column heuristic (DLX spirit)

6 AnnealPacker (anneal)
  ≈ Kirkpatrick SA on a packing layout
    (Dowsland-style packing SA; local window re-pack moves)
```


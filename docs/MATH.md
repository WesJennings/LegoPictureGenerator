# Math index

Short formula walkthroughs for the pipeline. Prose docs stay in the sibling
`.md` files; these MATH notes track the equations and search rules used in code.

Formulas are plain text / Unicode inside ` ```text ` blocks so they show in
Cursor’s Markdown preview (LaTeX `\( ... \)` is not rendered there).

| Doc | Topic | Code |
|---|---|---|
| [sampling/MATH.md](sampling/MATH.md) | Block sampling, stud grid, stud-count → block size | `image_sampler.cpp` |
| [color/MATH.md](color/MATH.md) | Nearest-color distance | `color_matcher.cpp` |
| [sizing/MATH.md](sizing/MATH.md) | Classic / stud-aim / piece-aim search | `piece_target.cpp`, `jobs.cpp` |
| [packing/MATH.md](packing/MATH.md) | Piece cardinality, DLX / Algorithm X sketch | `packers.cpp` |

C++ sources: `native/src/`. Host + engine: [native.md](native.md).

Also see: [image.md](image.md) · [color.md](color.md) · [packing.md](packing.md) ·
[algorithm-walkthrough.md](algorithm-walkthrough.md) · [api.md](api.md).

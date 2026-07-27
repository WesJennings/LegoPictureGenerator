# artifacts

Generated outputs from `make run`. Input photos stay in [`../Image/resources/`](../Image/resources/).

Contents of this folder (except `.gitkeep`) are **gitignored**.

## Files produced

| File | Source |
|------|--------|
| `output.png` | Flat color-matched stud mosaic (1 pixel per stud) |
| `output_lego.png` | `legoRender.renderStuds` — every stud as 1×1 |
| `output_lego_greedy.png` | Packed greedy |
| `output_lego_ilp.png` | Packed ILP B&B |
| `output_lego_rle.png` | Packed RLE + vertical merge |
| `output_lego_component.png` | Packed component-greedy |
| `output_lego_dlx.png` | Packed DLX / Algorithm X |
| `output_lego_anneal.png` | Packed simulated annealing |
| `color_counts.txt` | 1×1 color tally (`pieceCount`) |
| `bom_greedy.txt` | BOM — greedy |
| `bom_ilp.txt` | BOM — ILP |
| `bom_rle.txt` | BOM — RLE |
| `bom_component.txt` | BOM — component-greedy |
| `bom_dlx.txt` | BOM — DLX |
| `bom_anneal.txt` | BOM — anneal |
| `bom_compare.txt` | Multi-mode piece/time/status table |

The folder is created automatically by `loadImage` if missing.

## Related

- Pipeline: [`../Image/README.md`](../Image/README.md)
- Packing: [`../Pack/README.md`](../Pack/README.md)

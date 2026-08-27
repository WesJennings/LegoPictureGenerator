# Mosaic sizing math

**Goal:** pick a block size `B` (and thus a stud grid) from one of three modes.
Piece-aim additionally searches with repeated packs.

Formulas below use plain text / Unicode so they render in normal Markdown
preview (Cursor does not evaluate LaTeX `\( ... \)` blocks).

## Modes

| Mode | Control | Resolution |
|---|---|---|
| `fixed` | Classic width W* = 54 | Aspect-preserving `toStudGrid(54)` (not raw B=80, which collapses small uploads) |
| `auto` | target studs G* | `B = blockSizeForTargetStuds` (API only; see [sampling/MATH.md](../sampling/MATH.md)) |
| `pieces` | target pieces N | binary search on stud target + DLX probes (web UI) |

## Compression ratio

After packing, piece count `P` is usually much smaller than stud count `G`:

```text
R = P / G     where  0 < R ≤ 1
```

`R` depends on the photo, color regions, and packer. There is **no closed form**
for `P(G)`.

## Piece-aim search (`sizing=pieces`)

Forced packer: **DLX only**.

Constants (`JobConfig`):

- Initial ratio guess `R0 = 0.28`
- Relative tolerance `τ = 0.10`
- Max probes `K = 5`
- Stud search bounds `[Gmin, Gmax] = [400, 12000]`

### Initial stud guess

```text
G0 = clamp( round(N / R0), Gmin, Gmax )
```

### Probe

For a stud target `G`:

1. `B ← blockSizeForTargetStuds(W, H, G)`
2. Sample → color match → DLX pack (no renders)
3. Observe piece count `P`

### Binary search

Maintain stud bounds `[ℓ, h]`, start at `G = G0`. Each probe:

- If `P > N`: too many pieces → shrink grid → `h ← G − 1`
- If `P < N`: too few pieces → grow grid → `ℓ ← G + 1`
- Next `G ← floor( (ℓ + h) / 2 )`

Stop when `|P − N| / N ≤ τ` or after `K` probes. Keep the probe with
smallest `|P − N|`.

### Final pass

Run the full pipeline once with the winning `B` and DLX (render + BOM).
Final `P` can differ slightly from the best probe.

**Code:** `solvePieceTarget`, `probePieceCount`,
`JobService::resolveSizing` (`native/src/piece_target.cpp`, `pipeline.cpp`, `jobs.cpp`).

**Assumptions:** more studs usually ⇒ more pieces (used for search direction),
but packing can violate monotonicity. DLX may time out and fall back to greedy
on large blobs, so `P` is best-effort.

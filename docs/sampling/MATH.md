# Sampling math

**Goal:** turn a photo of size `W × H` pixels into a stud grid whose
dimensions (and therefore stud count) are controlled by a block size `B`.

Formulas use plain text / Unicode for normal Markdown preview.

## Classic block grid

With integer block size `B ≥ 1`:

```text
w = max(1, floor(W / B))
h = max(1, floor(H / B))
G = w · h
```

Each output stud averages the RGB of its `B × B` source block. Right/bottom
remainder pixels that do not fill a full block are dropped.

**Code:** `toStudGridByBlockSize`, `studCountFor`
(`native/src/image_sampler.cpp`).

**Defaults:** CLI / block-size jobs often use `B = 80` (`JobConfig.DEFAULT_BLOCK_SIZE`).
Valid range: `B ∈ [8, 256]`. Web Classic instead uses stud-width sampling
(`toStudGrid(54)`).

## Aim for stud count G*

Choose `B` so `G ≈ G*`:

1. Closed-form guess from total pixels `P = W · H`:

```text
B0 = round( √(P / G*) )
```

2. Clamp `B0` into `[Bmin, Bmax]`.
3. Local search over `B ∈ [B0−4, B0+4]` (still clamped); pick the `B` that
   minimizes `|G(B) − G*|`, breaking ties toward smaller `B`.

**Code:** `blockSizeForTargetStuds` (`native/src/image_sampler.cpp`).

**Assumptions:** aspect ratio is fixed by the source; exact `G*` is often
impossible because `w` and `h` are integers.

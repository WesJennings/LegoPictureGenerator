# data

Holds the Rebrickable-derived SQLite database used for colors and part availability.

## Expected file

| File | Role |
|------|------|
| `bricks.db` | SQLite DB (gitignored via `*.db`) |

Build or download via [rebrickable-sqlite](https://github.com/jncraton/rebrickable-sqlite) (or any dump with the same core tables).

## Tables used by this project

| Table | Used for |
|-------|----------|
| `colors` | `id`, `name`, `rgb`, `is_trans` — palette for matching |
| `elements` | `part_num` + `color_id` — which colors exist for a part |
| `parts` | Present in rebrickable dumps; not queried by current Java code |

### Callers

- [`Color/colorMatch`](../Color/README.md) — `loadElements` / `loadColors` (default part `3024`)
- [`Pack/PlateCatalog`](../Pack/README.md) — which plate sizes exist in which colors

## Path

Code expects `data/bricks.db` relative to the **project root** when you `make run`.

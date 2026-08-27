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
| `parts` | Optional / tooling (e.g. verifying plate names) |

### Callers

- [`ColorMatcher`](../docs/color.md) — `loadElements` / `loadColors` (default part `3024`)
- [`PlateCatalog`](../docs/packing.md) — which plate sizes exist in which colors

Both are loaded once at startup by `native/src/catalog.cpp`;
the server refuses to start without this file.

## Path

Code expects `data/bricks.db` relative to the **repo root** (where `make dev`
/ `make start` run). Override with the `LEGO_DB_PATH` environment variable.

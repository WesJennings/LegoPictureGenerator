# Lego Picture Generator

Turn any photo into a buildable LEGO mosaic — with a real parts list.

Upload a PNG or JPEG in the local web UI (or run the CLI) and get back:

- a stud-grid preview matched to real LEGO colors,
- a packed build that uses larger plates (2×4, 6×6, …) instead of thousands of 1×1s,
- a bill of materials (exact part numbers + colors to buy).

Everything runs on your machine. No accounts, no cloud, no telemetry.

## Quick start

Prerequisites: a C++17 compiler + CMake, SQLite 3 (`libsqlite3-dev`), Node 20+,
and `data/bricks.db` (see [`data/README.md`](data/README.md)). The mosaic
engine, HTTP API, job system, and CLI are C++; see [`docs/native.md`](docs/native.md).

```bash
make setup    # build C++ host + npm install
make start    # build everything, serve UI + API on http://127.0.0.1:8080
```

Open http://127.0.0.1:8080, drop in a photo, pick sizing / pack mode, done.

For development with hot reload:

```bash
make setup
make dev                  # terminal 1: C++ API on :8080
cd web && npm run dev     # terminal 2: Vite dev server on :5173 (proxies /api)
```

Other targets: `make test` (C++ tests + frontend type-check),
`make cli` (offline run on a sample image), `make clean`.

## Repository layout

| Path | What lives there |
|---|---|
| `native/` | C++ engine + HTTP host + CLI ([docs/native.md](docs/native.md)) |
| `web/` | React + TypeScript + Vite frontend ([README](web/README.md)) |
| `docs/` | Architecture, API reference, and algorithm deep-dives |
| `data/` | `bricks.db` SQLite catalog (not committed; [how to get it](data/README.md)) |
| `samples/` | Example input images |
| `runtime/` | Per-job uploads and outputs (gitignored, safe to delete) |

## Documentation

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — system diagrams: layers, jobs, pipeline, storage
- [`docs/native.md`](docs/native.md) — C++ engine and host, CUDA next steps
- [`docs/architecture.md`](docs/architecture.md) — ops detail: safety, retention, config
- [`docs/api.md`](docs/api.md) — HTTP API reference with curl examples
- [`docs/image.md`](docs/image.md) — image sampling and rendering
- [`docs/color.md`](docs/color.md) — LEGO color matching
- [`docs/packing.md`](docs/packing.md) — packing overview + research papers
- [`docs/algorithm-walkthrough.md`](docs/algorithm-walkthrough.md) — all six packing algorithms, step by step
- [`docs/MATH.md`](docs/MATH.md) — formula index (sampling, color, sizing, packing)

## How it works (30 seconds)

1. **Sample** — the photo is box-averaged down to a stud grid (classic: ~54 studs wide; or aim for a stud/piece target).
2. **Match** — every stud is matched to the nearest real LEGO color available as a 1×1 plate (Euclidean RGB over the `bricks.db` catalog).
3. **Pack** — same-color regions are tiled with the largest available plates. Six algorithms are implemented (greedy, ILP, RLE, component, DLX, anneal); piece-aim sizing always uses DLX with a multi-probe search.
4. **Render + BOM** — packed previews are drawn with stud texture and plate outlines, and the parts list is aggregated per (part, color).

## License

MIT — see [LICENSE](LICENSE).

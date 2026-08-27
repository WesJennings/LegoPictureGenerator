# Lego Picture Generator

Turn any photo into a buildable LEGO mosaic — with a real parts list.

Upload a PNG or JPEG in the local web UI (or run the CLI) and get back:

- a stud-grid preview matched to real LEGO colors,
- a packed build that uses larger plates (2×4, 6×6, …) instead of thousands of 1×1s,
- a bill of materials (exact part numbers + colors to buy).

Everything runs on your machine. No accounts, no cloud, no telemetry.

## Quick start

Prerequisites: Java 21+, a C++17 compiler + CMake, Node 20+, and `data/bricks.db` (see
[`data/README.md`](data/README.md)). Maven is downloaded automatically by the
wrapper. The mosaic engine (sample / match / pack / render) is C++ behind JNI;
see [`docs/native.md`](docs/native.md).

```bash
make setup    # resolve backend deps + npm install
make start    # build everything, serve UI + API on http://127.0.0.1:8080
```

Open http://127.0.0.1:8080, drop in a photo, pick sizing / pack mode, done.

For development with hot reload:

```bash
make dev                  # terminal 1: Java API on :8080
cd web && npm run dev     # terminal 2: Vite dev server on :5173 (proxies /api)
```

Other targets: `make test` (backend tests + frontend type-check),
`make cli` (offline run on a sample image), `make clean`.

## Repository layout

| Path | What lives there |
|---|---|
| `backend/` | Java 21 Maven host: job system, HTTP API, CLI, JNI wrappers ([README](backend/README.md)) |
| `native/` | C++ engine: sampling, color match, six packers, renderer ([docs/native.md](docs/native.md)) |
| `web/` | React + TypeScript + Vite frontend ([README](web/README.md)) |
| `docs/` | Architecture, API reference, and algorithm deep-dives |
| `data/` | `bricks.db` SQLite catalog (not committed; [how to get it](data/README.md)) |
| `samples/` | Example input images |
| `runtime/` | Per-job uploads and outputs (gitignored, safe to delete) |

## Documentation

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — system diagrams: layers, jobs, pipeline, storage
- [`docs/native.md`](docs/native.md) — C++ engine, JNI, CUDA next steps
- [`docs/architecture.md`](docs/architecture.md) — ops detail: safety, retention, config
- [`docs/api.md`](docs/api.md) — HTTP API reference with curl examples
- [`docs/image.md`](docs/image.md) — image sampling and rendering
- [`docs/color.md`](docs/color.md) — LEGO color matching
- [`docs/packing.md`](docs/packing.md) — packing overview + research papers
- [`docs/algorithm-walkthrough.md`](docs/algorithm-walkthrough.md) — all six packing algorithms, step by step
- [`docs/MATH.md`](docs/MATH.md) — formula index (sampling, color, sizing, packing)

## How it works (30 seconds)

1. **Sample** — the photo is box-averaged down to a stud grid (classic: block size 80; or aim for a stud/piece target).
2. **Match** — every stud is matched to the nearest real LEGO color available as a 1×1 plate (Euclidean RGB over the `bricks.db` catalog).
3. **Pack** — same-color regions are tiled with the largest available plates. Six algorithms are implemented (greedy, ILP, RLE, component, DLX, anneal); piece-aim sizing always uses DLX with a multi-probe search.
4. **Render + BOM** — packed previews are drawn with stud texture and plate outlines, and the parts list is aggregated per (part, color).

## License

MIT — see [LICENSE](LICENSE).

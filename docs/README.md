# Docs

Start at the root [`ARCHITECTURE.md`](../ARCHITECTURE.md) for **repository
layout**, system diagrams, and “where to change things.”

This folder is the long-form reference. Nothing here is required to run the app.

| Doc | What it covers |
|-----|----------------|
| [architecture.md](architecture.md) | Ops: safety table, retention, env vars |
| [deploy.md](deploy.md) | Hosting publicly: Docker image, Cloudflare Tunnel from a PC, or Caddy on a VM |
| [native.md](native.md) | C++ engine + host, parity with the original Java, CUDA next |
| [api.md](api.md) | HTTP contract (`/api/v1`) with curl examples |
| [image.md](image.md) | Sampling and rendering |
| [color.md](color.md) | LEGO color matching (OKLab) |
| [packing.md](packing.md) | Packing overview, types, BOM, research papers |
| [algorithm-walkthrough.md](algorithm-walkthrough.md) | The six packers, step by step |
| [MATH.md](MATH.md) | Formula index → sampling / color / sizing / packing |
| [sizing/MATH.md](sizing/MATH.md) | Classic / stud-aim / piece-aim search |

Code maps:

- C++ files: [`../native/README.md`](../native/README.md)
- UI files: [`../web/README.md`](../web/README.md)
- Catalog: [`../data/README.md`](../data/README.md)

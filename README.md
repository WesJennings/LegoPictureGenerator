# LegoPictureGenerator

Turns a photo into a LEGO-plate mosaic: downscale → match Rebrickable colors → optional plate packing → BOM + LEGO-look renders.

## Quick start

From the project root (requires `data/bricks.db` and `lib/sqlite-jdbc-*.jar`):

```bash
make        # compile
make run    # compile + run Image/loadImage
make clean  # remove .class files
```

Input PNGs live in `Image/resources/`. All generated files go to `artifacts/`.

## Pipeline

```text
Image/resources/*.png
        │
        ▼
   Image/loadImage          MergePixels → ReScale (BLOCK_SIZE)
        │
        ▼
   Color/colorMatch         nearest opaque 3024 color per stud
        │
        ├─► pieceCount      1×1 color tally
        │
        ├─► Pack/           six packing modes → BOM + compare
        │
        └─► legoRender      flat + packed LEGO-look PNGs
                │
                ▼
           artifacts/
```

## Project sections

| Folder | Role | Docs |
|--------|------|------|
| [`Color/`](Color/) | SQLite palette + nearest-color match | [README](Color/README.md) |
| [`Image/`](Image/) | Main entry, downscale, piece counts, 2D render | [README](Image/README.md) |
| [`Pack/`](Pack/) | Plate catalog, six packers, BOM compare | [README](Pack/README.md) · [walkthrough](Pack/ALGORITHM_WALKTHROUGH.md) |
| [`artifacts/`](artifacts/) | Generated outputs (gitignored contents) | [README](artifacts/README.md) |
| [`data/`](data/) | Rebrickable SQLite DB (`bricks.db`) | [README](data/README.md) |
| [`lib/`](lib/) | JDBC driver JAR | [README](lib/README.md) |

## Requirements

- Java (javac/java on `PATH`)
- [`data/bricks.db`](data/README.md) from [rebrickable-sqlite](https://github.com/jncraton/rebrickable-sqlite) (or equivalent schema)
- [`lib/sqlite-jdbc-3.53.2.0.jar`](lib/README.md)

## License

See [LICENSE](LICENSE).

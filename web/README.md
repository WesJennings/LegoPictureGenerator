# Web frontend

React + TypeScript + Vite single-page app. Pure API client — all processing
happens in the C++ host (`lego_server`).

## Develop

```bash
npm install
npm run dev     # http://localhost:5173, proxies /api → 127.0.0.1:8080
```

Start the backend first (`make setup && make dev` at the repo root). The Vite
proxy means no CORS configuration anywhere.

## Build

```bash
npm run build   # type-checks, then emits dist/
```

`lego_server` serves `web/dist/` automatically when it exists, so after
`make build` the whole app is one C++ process on
http://127.0.0.1:8080.

## Structure

```
src/
├── main.tsx            # router: / (create) and /jobs/:jobId (results)
├── App.tsx             # shell layout
├── packModes.ts        # pack mode ids + compare-all → API `modes` string
├── api/
│   ├── jobs.ts         # createJob / getJob fetch wrappers
│   └── types.ts        # mirrors the backend JSON contract (docs/api.md)
├── pages/
│   ├── CreateJobPage   # dropzone + sizing + pack mode + stud-BOM toggle
│   └── JobPage         # 750ms polling until COMPLETE/FAILED, then results
├── components/
│   ├── ImageDropzone   # drag-drop / file picker with preview
│   ├── MosaicSizing    # fixed / pieces sizing controls
│   ├── PackModeSelect  # single mode or compare-all
│   ├── JobProgress     # pipeline stage chips
│   ├── PreviewGallery  # packed build / stud mosaic / matched flat
│   ├── CompareResults  # side-by-side when multiple modes ran
│   ├── ResultSummary   # grid size, piece count, colors
│   ├── BomTable        # parts list + .txt download
│   └── ErrorNotice
└── styles/app.css      # plain CSS, dark theme
```

Artifacts (preview PNGs, BOM downloads) are plain URLs returned by the job
endpoint — the browser loads them directly from the host.

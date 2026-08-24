# HTTP API

Base URL: `http://127.0.0.1:8080`. All endpoints are under `/api/v1`.
JSON errors always have the shape `{"error": "message"}`.

Requests must carry a local `Host` header (`localhost` or `127.0.0.1`);
anything else gets `403`.

## POST /api/v1/jobs

Create a job. Multipart form:

| Field | Required | Notes |
|---|---|---|
| `image` | yes | PNG or JPEG, ≤ 25 MB, ≤ 25 megapixels (checked by magic bytes + header, not extension) |
| `modes` | no | comma-separated: `greedy,ilp,rle,component,dlx,anneal`; default `greedy`. Ignored when `sizing=pieces` (forced to `dlx`). |
| `sizing` | no | `fixed` (default Classic ≈54 studs wide), `auto` (aim for stud count), or `pieces` (aim for packed pieces via DLX search) |
| `targetStudCount` | if `sizing=auto` | 400–12000; default 4000 |
| `targetPieceCount` | if `sizing=pieces` | 150–4000; default 800. Best-effort within ~10% after up to 5 DLX probes (can take several minutes). |
| `includeStudBom` | no | `true` / `1` / `on` — also emit a 1×1 stud parts list (`result.studBom`, artifact `bomStuds`) |

Math for sizing/search: [`docs/sizing/MATH.md`](sizing/MATH.md).

```bash
curl -X POST \
  -F image=@samples/Jarvis.png \
  -F sizing=pieces \
  -F targetPieceCount=800 \
  http://127.0.0.1:8080/api/v1/jobs
```

`202 Accepted`:

```json
{ "jobId": "9bf14f5f-…", "statusUrl": "/api/v1/jobs/9bf14f5f-…" }
```

Errors: `400` (bad file/params), `429` (queue full, retry later).

## GET /api/v1/jobs/{jobId}

Poll job state. `status` walks
`QUEUED → [SEARCHING →] PREPROCESSING → MATCHING → PACKING → RENDERING → COMPLETE`
(or `FAILED` with `error` set). `SEARCHING` appears only for `sizing=pieces`.
Poll every ~750 ms until terminal.

`settings` includes `sizingMode`, `targetStudCount`, `targetPieceCount`,
`pieceSearchAttempts`, `achievedPieceCount`, `autoSizing` (legacy bool for stud-aim),
`blockSize`, `modes`.

`200`:

```json
{
  "jobId": "9bf14f5f-…",
  "status": "COMPLETE",
  "error": null,
  "createdAtEpochMs": 1753988000000,
  "updatedAtEpochMs": 1753988003000,
  "settings": { "blockSize": 80, "renderStudSizePx": 24, "modes": ["greedy"] },
  "result": {
    "gridWidth": 54,
    "gridHeight": 72,
    "totalStuds": 3888,
    "colorCounts": [ { "colorId": 0, "colorName": "Black", "rgbHex": "05131D", "count": 1450 } ],
    "modeResults": [
      {
        "mode": "greedy",
        "status": "ok",
        "elapsedMs": 25,
        "pieceCount": 1335,
        "bom": [ { "partNum": "3036", "w": 6, "h": 8, "colorId": 0, "colorName": "Black", "count": 12 } ]
      }
    ],
    "artifacts": { "matched": "matched.png", "legoGreedy": "lego-greedy.png" }
  },
  "artifacts": {
    "matched": "/api/v1/jobs/9bf14f5f-…/artifacts/matched.png",
    "legoStuds": "/api/v1/jobs/9bf14f5f-…/artifacts/lego-studs.png",
    "legoGreedy": "/api/v1/jobs/9bf14f5f-…/artifacts/lego-greedy.png",
    "bomGreedy": "/api/v1/jobs/9bf14f5f-…/artifacts/bom-greedy.txt",
    "placementsGreedy": "/api/v1/jobs/9bf14f5f-…/artifacts/placements-greedy.json",
    "colorCounts": "/api/v1/jobs/9bf14f5f-…/artifacts/color-counts.txt"
  }
}
```

`404` if the ID is unknown (or not a UUID).

## GET /api/v1/jobs/{jobId}/artifacts/{name}

Download one artifact. Only filenames listed in the job's manifest are
servable — anything else (including `manifest.json` and traversal attempts) is
`404`. PNGs are served inline; text/JSON artifacts as attachments. All
responses carry `X-Content-Type-Options: nosniff`.

## DELETE /api/v1/jobs/{jobId}

Delete a finished job and its files. `204` on success, `409` if the job is
still queued or running.

## GET /api/v1/health

`200` with `{"status":"ok","dbPath":"data/bricks.db","paletteColors":67}`.
Startup fails outright if `bricks.db` is missing, so a running server implies
a loaded catalog.

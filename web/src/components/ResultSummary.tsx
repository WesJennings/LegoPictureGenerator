import type { JobResponse } from "../api/types";
import { packModeLabel } from "../packModes";

export default function ResultSummary({ job }: { job: JobResponse }) {
  const result = job.result;
  if (!result) {
    return null;
  }
  const modeName = job.settings.modes[0] ?? result.modeResults[0]?.mode;
  const mode = result.modeResults.find((m) => m.mode === modeName)
    ?? result.modeResults[0];
  const sizing = job.settings.sizingMode ?? (job.settings.autoSizing ? "auto" : "fixed");

  return (
    <dl className="summary">
      <div>
        <dt>Grid</dt>
        <dd>
          {result.gridWidth} × {result.gridHeight} studs
        </dd>
      </div>
      <div>
        <dt>Total studs</dt>
        <dd>{result.totalStuds.toLocaleString()}</dd>
      </div>
      <div>
        <dt>Sizing</dt>
        <dd>
          {sizing === "fixed" || (job.settings.targetStudWidth ?? 0) > 0
            ? `Classic ~${job.settings.targetStudWidth || 54} studs wide`
            : `${job.settings.blockSize}px / stud`}
          {sizing === "auto"
            ? ` (aim ~${job.settings.targetStudCount.toLocaleString()} studs)`
            : ""}
          {sizing === "pieces"
            ? ` · ${job.settings.pieceSearchAttempts || "?"} probes`
            : ""}
        </dd>
      </div>
      {sizing === "pieces" && (
        <div>
          <dt>Piece aim</dt>
          <dd>
            ~{job.settings.targetPieceCount.toLocaleString()}
            {mode
              ? ` → ${mode.pieceCount.toLocaleString()} packed`
              : job.settings.achievedPieceCount
                ? ` → ${job.settings.achievedPieceCount.toLocaleString()} (probe)`
                : ""}
          </dd>
        </div>
      )}
      {mode && (
        <>
          <div>
            <dt>Algorithm</dt>
            <dd>{packModeLabel(mode.mode)}</dd>
          </div>
          <div>
            <dt>Pieces to buy</dt>
            <dd>{mode.pieceCount.toLocaleString()}</dd>
          </div>
          <div>
            <dt>Packing time</dt>
            <dd>{mode.elapsedMs} ms</dd>
          </div>
        </>
      )}
      <div>
        <dt>Colors used</dt>
        <dd>{result.colorCounts.length}</dd>
      </div>
    </dl>
  );
}

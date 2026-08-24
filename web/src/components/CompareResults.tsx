import type { JobResponse, ModeResult } from "../api/types";
import { artifactKey, packModeLabel } from "../packModes";
import BomTable from "./BomTable";

interface Props {
  job: JobResponse;
}

export default function CompareResults({ job }: Props) {
  const result = job.result;
  if (!result) {
    return null;
  }

  const modes = result.modeResults;
  const baseline = modes[0];
  const bestPieces = Math.min(...modes.map((m) => m.pieceCount));
  const fastest = Math.min(...modes.map((m) => m.elapsedMs));

  return (
    <section className="compare">
      <header className="compare-header">
        <h2>Algorithm comparison</h2>
        <p className="muted">
          Grid {result.gridWidth} × {result.gridHeight} (
          {result.totalStuds.toLocaleString()} studs)
          {job.settings.sizingMode === "auto" || job.settings.autoSizing
            ? ` · auto block ${job.settings.blockSize}px (aim ~${job.settings.targetStudCount.toLocaleString()})`
            : job.settings.sizingMode === "pieces"
              ? ` · piece aim ~${job.settings.targetPieceCount.toLocaleString()} (block ${job.settings.blockSize}px)`
              : ` · block ${job.settings.blockSize}px`}
          {" · "}
          {result.colorCounts.length} colors · deltas vs{" "}
          {packModeLabel(baseline.mode)}
        </p>
      </header>

      <div className="compare-table-wrap">
        <table className="compare-table">
          <thead>
            <tr>
              <th>Algorithm</th>
              <th>Pieces</th>
              <th>vs {packModeLabel(baseline.mode)}</th>
              <th>Time</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            {modes.map((m) => (
              <StatsRow
                key={m.mode}
                mode={m}
                baselinePieces={baseline.pieceCount}
                bestPieces={bestPieces}
                fastest={fastest}
              />
            ))}
          </tbody>
        </table>
      </div>

      <div className="compare-grid">
        {modes.map((m) => {
          const imageUrl = job.artifacts[artifactKey("lego", m.mode)];
          const bomUrl = job.artifacts[artifactKey("bom", m.mode)];
          const delta = m.pieceCount - baseline.pieceCount;
          return (
            <article
              key={m.mode}
              className={`compare-card ${
                m.pieceCount === bestPieces ? "best" : ""
              }`}
            >
              <header className="compare-card-header">
                <h3>{packModeLabel(m.mode)}</h3>
                {m.pieceCount === bestPieces && (
                  <span className="compare-badge">Fewest pieces</span>
                )}
              </header>
              {imageUrl ? (
                <a href={imageUrl} target="_blank" rel="noreferrer">
                  <img src={imageUrl} alt={`${m.mode} packed mosaic`} />
                </a>
              ) : (
                <div className="compare-missing muted">No preview</div>
              )}
              <dl className="compare-stats">
                <div>
                  <dt>Pieces</dt>
                  <dd>{m.pieceCount.toLocaleString()}</dd>
                </div>
                <div>
                  <dt>Delta</dt>
                  <dd className={deltaClass(delta)}>
                    {formatDelta(delta)}
                  </dd>
                </div>
                <div>
                  <dt>Time</dt>
                  <dd>{formatMs(m.elapsedMs)}</dd>
                </div>
                <div>
                  <dt>Status</dt>
                  <dd className="compare-status">{m.status}</dd>
                </div>
              </dl>
              {bomUrl && (
                <a className="download" href={bomUrl}>
                  Download BOM
                </a>
              )}
            </article>
          );
        })}
      </div>
      {job.settings.includeStudBom &&
        result.studBom &&
        result.studBom.length > 0 && (
          <BomTable
            rows={result.studBom}
            mode="studs"
            title="1×1 stud parts list"
            downloadUrl={job.artifacts.bomStuds}
          />
        )}
    </section>
  );
}

function StatsRow({
  mode,
  baselinePieces,
  bestPieces,
  fastest,
}: {
  mode: ModeResult;
  baselinePieces: number;
  bestPieces: number;
  fastest: number;
}) {
  const delta = mode.pieceCount - baselinePieces;
  return (
    <tr>
      <td>{packModeLabel(mode.mode)}</td>
      <td className={mode.pieceCount === bestPieces ? "stat-best" : undefined}>
        {mode.pieceCount.toLocaleString()}
      </td>
      <td className={deltaClass(delta)}>{formatDelta(delta)}</td>
      <td className={mode.elapsedMs === fastest ? "stat-best" : undefined}>
        {formatMs(mode.elapsedMs)}
      </td>
      <td className="compare-status">{mode.status}</td>
    </tr>
  );
}

function formatDelta(delta: number): string {
  if (delta === 0) {
    return "—";
  }
  return (delta > 0 ? "+" : "") + delta.toLocaleString();
}

function deltaClass(delta: number): string | undefined {
  if (delta < 0) {
    return "delta-better";
  }
  if (delta > 0) {
    return "delta-worse";
  }
  return undefined;
}

function formatMs(ms: number): string {
  if (ms < 1000) {
    return `${ms} ms`;
  }
  return `${(ms / 1000).toFixed(1)} s`;
}

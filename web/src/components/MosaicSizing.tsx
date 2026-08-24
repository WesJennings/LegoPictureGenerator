export type SizingMode = "fixed" | "pieces";

export const DEFAULT_TARGET_PIECES = 800;
export const MIN_TARGET_PIECES = 150;
export const MAX_TARGET_PIECES = 4000;

interface Props {
  sizing: SizingMode;
  onSizing: (value: SizingMode) => void;
  targetPieces: number;
  onTargetPieces: (value: number) => void;
}

export default function MosaicSizing({
  sizing,
  onSizing,
  targetPieces,
  onTargetPieces,
}: Props) {
  return (
    <div className="settings mosaic-sizing">
      <span className="settings-label">Mosaic size</span>
      <div className="sizing-options" role="radiogroup" aria-label="Mosaic size">
        <label className={`sizing-option ${sizing === "fixed" ? "active" : ""}`}>
          <input
            type="radio"
            name="sizing"
            value="fixed"
            checked={sizing === "fixed"}
            onChange={() => onSizing("fixed")}
          />
          <span className="sizing-option-body">
            <span className="sizing-option-title">Classic</span>
            <span className="muted">
              Standard mosaic about 54 studs wide (height follows your photo).
            </span>
          </span>
        </label>
        <label className={`sizing-option ${sizing === "pieces" ? "active" : ""}`}>
          <input
            type="radio"
            name="sizing"
            value="pieces"
            checked={sizing === "pieces"}
            onChange={() => onSizing("pieces")}
          />
          <span className="sizing-option-body">
            <span className="sizing-option-title">Aim for N pieces</span>
            <span className="muted">
              Searches with DLX (up to several pack runs) to land near your
              piece count — slower, best-effort within ~10%.
            </span>
          </span>
        </label>
      </div>

      {sizing === "pieces" && (
        <div className="target-studs">
          <label htmlFor="target-pieces">
            Target pieces: <strong>{targetPieces.toLocaleString()}</strong>
          </label>
          <input
            id="target-pieces"
            type="range"
            min={MIN_TARGET_PIECES}
            max={MAX_TARGET_PIECES}
            step={50}
            value={targetPieces}
            onChange={(e) => onTargetPieces(Number(e.target.value))}
          />
          <p className="muted">
            Uses DLX only. May take several minutes while it probes different
            mosaic sizes. Exact hit is not guaranteed.
          </p>
        </div>
      )}
    </div>
  );
}

interface Props {
  studWidth: number;
  onStudWidth: (value: number) => void;
}

export default function MosaicSettings({ studWidth, onStudWidth }: Props) {
  return (
    <div className="settings">
      <label htmlFor="stud-width">
        Mosaic width: <strong>{studWidth} studs</strong>
      </label>
      <input
        id="stud-width"
        type="range"
        min={16}
        max={128}
        value={studWidth}
        onChange={(e) => onStudWidth(Number(e.target.value))}
      />
      <p className="muted">
        Height follows your photo's aspect ratio. Wider = more detail, more
        pieces.
      </p>
    </div>
  );
}

import type { BomRow } from "../api/types";
import { packModeLabel } from "../packModes";

interface Props {
  rows: BomRow[];
  mode: string;
  /** Override the default "Parts list (Mode)" heading. */
  title?: string;
  downloadUrl?: string;
}

export default function BomTable({ rows, mode, title, downloadUrl }: Props) {
  return (
    <div className="bom">
      <div className="bom-header">
        <h2>{title ?? `Parts list (${packModeLabel(mode)})`}</h2>
        {downloadUrl && (
          <a className="download" href={downloadUrl}>
            Download .txt
          </a>
        )}
      </div>
      <table>
        <thead>
          <tr>
            <th>Count</th>
            <th>Part</th>
            <th>Size</th>
            <th>Color</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((row) => (
            <tr key={`${row.partNum}-${row.colorId}`}>
              <td>{row.count}</td>
              <td>{row.partNum}</td>
              <td>
                {row.w}×{row.h}
              </td>
              <td>{row.colorName}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

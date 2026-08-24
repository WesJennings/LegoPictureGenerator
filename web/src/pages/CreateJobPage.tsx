import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { createJob } from "../api/jobs";
import ImageDropzone from "../components/ImageDropzone";
import PackModeSelect from "../components/PackModeSelect";
import MosaicSizing, {
  DEFAULT_TARGET_PIECES,
  type SizingMode,
} from "../components/MosaicSizing";
import ErrorNotice from "../components/ErrorNotice";
import { DEFAULT_PACK_MODE } from "../packModes";

export default function CreateJobPage() {
  const navigate = useNavigate();
  const [file, setFile] = useState<File | null>(null);
  const [mode, setMode] = useState(DEFAULT_PACK_MODE);
  const [sizing, setSizing] = useState<SizingMode>("fixed");
  const [targetPieces, setTargetPieces] = useState(DEFAULT_TARGET_PIECES);
  const [includeStudBom, setIncludeStudBom] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function submit() {
    if (!file) {
      return;
    }
    setSubmitting(true);
    setError(null);
    try {
      const packMode = sizing === "pieces" ? "dlx" : mode;
      const { jobId } = await createJob(file, packMode, {
        sizing,
        targetPieceCount: targetPieces,
        includeStudBom,
      });
      navigate(`/jobs/${jobId}`);
    } catch (e) {
      setError(e instanceof Error ? e.message : "Upload failed");
      setSubmitting(false);
    }
  }

  return (
    <section className="create-page">
      <h1>Turn a photo into a LEGO mosaic</h1>
      <p className="lede">
        Upload a PNG or JPEG. We match every stud to a real LEGO color, pack it
        into buildable plates, and give you the parts list.
      </p>

      <ImageDropzone file={file} onFile={setFile} />
      <MosaicSizing
        sizing={sizing}
        onSizing={setSizing}
        targetPieces={targetPieces}
        onTargetPieces={setTargetPieces}
      />
      {sizing !== "pieces" && (
        <PackModeSelect mode={mode} onMode={setMode} />
      )}

      <label className="toggle-option">
        <input
          type="checkbox"
          checked={includeStudBom}
          onChange={(e) => setIncludeStudBom(e.target.checked)}
        />
        <span className="toggle-option-body">
          <span className="toggle-option-title">
            Also include 1×1 stud parts list
          </span>
          <span className="muted">
            Adds a second shopping list with every stud as its own plate (before
            packing into larger pieces).
          </span>
        </span>
      </label>

      {error && <ErrorNotice message={error} />}

      <button
        className="primary"
        disabled={!file || submitting}
        onClick={submit}
      >
        {submitting ? "Uploading…" : "Generate mosaic"}
      </button>
    </section>
  );
}

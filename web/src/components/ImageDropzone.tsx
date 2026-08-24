import { useCallback, useEffect, useState } from "react";

interface Props {
  file: File | null;
  onFile: (file: File | null) => void;
}

export default function ImageDropzone({ file, onFile }: Props) {
  const [previewUrl, setPreviewUrl] = useState<string | null>(null);
  const [dragOver, setDragOver] = useState(false);

  useEffect(() => {
    if (!file) {
      setPreviewUrl(null);
      return;
    }
    const url = URL.createObjectURL(file);
    setPreviewUrl(url);
    return () => URL.revokeObjectURL(url);
  }, [file]);

  const accept = useCallback(
    (candidate: File | undefined) => {
      if (!candidate) {
        return;
      }
      if (!/image\/(png|jpeg)/.test(candidate.type)) {
        return;
      }
      onFile(candidate);
    },
    [onFile],
  );

  return (
    <label
      className={`dropzone ${dragOver ? "drag-over" : ""}`}
      onDragOver={(e) => {
        e.preventDefault();
        setDragOver(true);
      }}
      onDragLeave={() => setDragOver(false)}
      onDrop={(e) => {
        e.preventDefault();
        setDragOver(false);
        accept(e.dataTransfer.files[0]);
      }}
    >
      {previewUrl ? (
        <img src={previewUrl} alt="Selected upload" className="dropzone-preview" />
      ) : (
        <span>Drop a PNG or JPEG here, or click to choose (max 25 MB)</span>
      )}
      <input
        type="file"
        accept="image/png,image/jpeg"
        hidden
        onChange={(e) => accept(e.target.files?.[0])}
      />
    </label>
  );
}

import { artifactKey } from "../packModes";

interface Props {
  artifacts: Record<string, string>;
  mode: string;
}

export default function PreviewGallery({ artifacts, mode }: Props) {
  const views = [
    { key: artifactKey("lego", mode), label: "Packed LEGO build" },
    { key: "legoStuds", label: "1×1 stud mosaic" },
  ].filter((v) => artifacts[v.key]);

  if (views.length === 0) {
    return null;
  }
  return (
    <div className="gallery">
      {views.map((v) => (
        <figure key={v.key}>
          <a href={artifacts[v.key]} target="_blank" rel="noreferrer">
            <img src={artifacts[v.key]} alt={v.label} />
          </a>
          <figcaption>{v.label}</figcaption>
        </figure>
      ))}
    </div>
  );
}

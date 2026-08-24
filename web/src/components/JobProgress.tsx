import type { JobStatus, SizingMode } from "../api/types";
import ErrorNotice from "./ErrorNotice";

const BASE_STAGES: { key: JobStatus; label: string }[] = [
  { key: "QUEUED", label: "Queued" },
  { key: "PREPROCESSING", label: "Reading photo" },
  { key: "MATCHING", label: "Matching LEGO colors" },
  { key: "PACKING", label: "Packing plates" },
  { key: "RENDERING", label: "Rendering previews" },
  { key: "COMPLETE", label: "Done" },
];

const SEARCH_STAGE = {
  key: "SEARCHING" as JobStatus,
  label: "Tuning piece count",
};

interface Props {
  status: JobStatus;
  error: string | null;
  sizingMode?: SizingMode;
}

export default function JobProgress({ status, error, sizingMode }: Props) {
  if (status === "FAILED") {
    return <ErrorNotice message={error ?? "Job failed"} />;
  }

  const stages =
    sizingMode === "pieces"
      ? [
          BASE_STAGES[0],
          SEARCH_STAGE,
          ...BASE_STAGES.slice(1),
        ]
      : BASE_STAGES;

  const loadingStages = stages.filter((s) => s.key !== "COMPLETE");
  const activeIndex = Math.max(
    0,
    loadingStages.findIndex((s) => s.key === status),
  );
  const done = status === "COMPLETE";
  const stage =
    stages.find((s) => s.key === status) ?? loadingStages[activeIndex];

  const size = 88;
  const stroke = 5;
  const radius = (size - stroke) / 2;
  const circumference = 2 * Math.PI * radius;
  const arcLength = done ? circumference : circumference * 0.28;

  return (
    <div className={`loader ${done ? "loader-done" : ""}`} aria-live="polite">
      <div className="loader-ring-wrap">
        <svg
          className={`loader-ring ${done ? "" : "spinning"}`}
          width={size}
          height={size}
          viewBox={`0 0 ${size} ${size}`}
          aria-hidden="true"
        >
          <circle
            className="loader-track"
            cx={size / 2}
            cy={size / 2}
            r={radius}
            strokeWidth={stroke}
            fill="none"
          />
          <circle
            className="loader-arc"
            cx={size / 2}
            cy={size / 2}
            r={radius}
            strokeWidth={stroke}
            fill="none"
            strokeDasharray={`${arcLength} ${circumference}`}
            strokeLinecap="round"
          />
        </svg>
        {done ? (
          <span className="loader-check" aria-hidden="true">
            ✓
          </span>
        ) : (
          <span className="loader-pulse" aria-hidden="true" />
        )}
      </div>

      <div className="loader-copy">
        <p key={stage.key} className="loader-stage">
          {stage.label}
        </p>
        {!done && (
          <ol className="loader-steps">
            {loadingStages.map((s, i) => (
              <li
                key={s.key}
                className={
                  i < activeIndex ? "done" : i === activeIndex ? "active" : ""
                }
              >
                <span className="loader-dot" />
                <span className="loader-step-label">{s.label}</span>
              </li>
            ))}
          </ol>
        )}
      </div>
    </div>
  );
}

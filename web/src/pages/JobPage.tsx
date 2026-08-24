import { useEffect, useState } from "react";
import { useParams } from "react-router-dom";
import { getJob } from "../api/jobs";
import type { JobResponse } from "../api/types";
import JobProgress from "../components/JobProgress";
import PreviewGallery from "../components/PreviewGallery";
import ResultSummary from "../components/ResultSummary";
import BomTable from "../components/BomTable";
import CompareResults from "../components/CompareResults";
import ErrorNotice from "../components/ErrorNotice";
import { artifactKey, isCompareJob } from "../packModes";

const POLL_MS = 750;

export default function JobPage() {
  const { jobId } = useParams<{ jobId: string }>();
  const [job, setJob] = useState<JobResponse | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (!jobId) {
      return;
    }
    let cancelled = false;
    let timer: number | undefined;

    async function poll() {
      try {
        const next = await getJob(jobId!);
        if (cancelled) {
          return;
        }
        setJob(next);
        if (next.status !== "COMPLETE" && next.status !== "FAILED") {
          timer = window.setTimeout(poll, POLL_MS);
        }
      } catch (e) {
        if (!cancelled) {
          setError(e instanceof Error ? e.message : "Could not load job");
        }
      }
    }

    poll();
    return () => {
      cancelled = true;
      if (timer !== undefined) {
        window.clearTimeout(timer);
      }
    };
  }, [jobId]);

  if (error) {
    return <ErrorNotice message={error} />;
  }
  if (!job) {
    return <p className="muted">Loading job…</p>;
  }

  const compare = isCompareJob(job.settings.modes);
  const mode = job.settings.modes[0]
    ?? job.result?.modeResults[0]?.mode
    ?? "greedy";
  const modeResult = job.result?.modeResults.find((m) => m.mode === mode);

  return (
    <section className="job-page">
      <JobProgress
        status={job.status}
        error={job.error}
        sizingMode={job.settings.sizingMode}
      />
      {job.status === "COMPLETE" && job.result && (
        compare ? (
          <CompareResults job={job} />
        ) : (
          <>
            <PreviewGallery artifacts={job.artifacts} mode={mode} />
            <ResultSummary job={job} />
            {modeResult && (
              <BomTable
                rows={modeResult.bom}
                mode={mode}
                downloadUrl={job.artifacts[artifactKey("bom", mode)]}
              />
            )}
            {job.settings.includeStudBom &&
              job.result.studBom &&
              job.result.studBom.length > 0 && (
                <BomTable
                  rows={job.result.studBom}
                  mode="studs"
                  title="1×1 stud parts list"
                  downloadUrl={job.artifacts.bomStuds}
                />
              )}
          </>
        )
      )}
    </section>
  );
}

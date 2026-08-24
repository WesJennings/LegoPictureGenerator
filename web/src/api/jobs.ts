import type { JobResponse } from "./types";
import { modesForRequest } from "../packModes";
import type { SizingMode } from "../components/MosaicSizing";

async function parseError(res: Response): Promise<never> {
  let message = `Request failed (${res.status})`;
  try {
    const body = await res.json();
    if (body && typeof body.error === "string") {
      message = body.error;
    }
  } catch {
    // keep the fallback message
  }
  throw new Error(message);
}

export interface CreateJobOptions {
  sizing: SizingMode;
  targetPieceCount: number;
  includeStudBom: boolean;
}

export async function createJob(
  image: File,
  mode: string,
  options: CreateJobOptions,
): Promise<{ jobId: string }> {
  const form = new FormData();
  form.append("image", image);
  form.append("modes", modesForRequest(mode));
  form.append("sizing", options.sizing);
  if (options.sizing === "pieces") {
    form.append("targetPieceCount", String(options.targetPieceCount));
  }
  if (options.includeStudBom) {
    form.append("includeStudBom", "true");
  }
  const res = await fetch("/api/v1/jobs", { method: "POST", body: form });
  if (!res.ok) {
    return parseError(res);
  }
  return res.json();
}

export async function getJob(jobId: string): Promise<JobResponse> {
  const res = await fetch(`/api/v1/jobs/${jobId}`);
  if (!res.ok) {
    return parseError(res);
  }
  return res.json();
}

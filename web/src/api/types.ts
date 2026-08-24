export type JobStatus =
  | "QUEUED"
  | "SEARCHING"
  | "PREPROCESSING"
  | "MATCHING"
  | "PACKING"
  | "RENDERING"
  | "COMPLETE"
  | "FAILED";

/** UI create modes are fixed | pieces; API may still return auto for older jobs. */
export type SizingMode = "fixed" | "auto" | "pieces";

export interface ColorCountRow {
  colorId: number;
  colorName: string;
  rgbHex: string;
  count: number;
}

export interface BomRow {
  partNum: string;
  w: number;
  h: number;
  colorId: number;
  colorName: string;
  count: number;
}

export interface ModeResult {
  mode: string;
  status: string;
  elapsedMs: number;
  pieceCount: number;
  bom: BomRow[];
}

export interface PipelineResult {
  gridWidth: number;
  gridHeight: number;
  totalStuds: number;
  colorCounts: ColorCountRow[];
  studBom: BomRow[];
  modeResults: ModeResult[];
  artifacts: Record<string, string>;
}

export interface JobResponse {
  jobId: string;
  status: JobStatus;
  error: string | null;
  createdAtEpochMs: number;
  updatedAtEpochMs: number;
  settings: {
    blockSize: number;
    targetStudWidth: number;
    sizingMode: SizingMode;
    autoSizing: boolean;
    targetStudCount: number;
    targetPieceCount: number;
    pieceSearchAttempts: number;
    achievedPieceCount: number;
    includeStudBom: boolean;
    renderStudSizePx: number;
    modes: string[];
  };
  result: PipelineResult | null;
  artifacts: Record<string, string>;
}

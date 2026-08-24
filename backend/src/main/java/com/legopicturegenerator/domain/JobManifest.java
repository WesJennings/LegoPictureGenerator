package com.legopicturegenerator.domain;

import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Persistent state of one job; serialized atomically to manifest.json. */
@JsonIgnoreProperties(ignoreUnknown = true)
public class JobManifest {
  public String id;
  public String status = JobStatus.QUEUED.name();
  public String error;
  public long createdAtEpochMs;
  public long updatedAtEpochMs;
  /** Pixels per stud when downscaling (legacy BLOCK_SIZE). Resolved before/while running. */
  public int blockSize;
  /**
   * When &gt; 0, Classic/web sampling uses stud-width grid instead of {@link #blockSize}
   * (typically {@link JobConfig#CLASSIC_STUD_WIDTH}).
   */
  public int targetStudWidth;
  /**
   * Sizing mode: {@code fixed}, {@code auto} (stud target), or {@code pieces}
   * (DLX piece-target search). Older manifests may omit this and use {@link #autoSizing}.
   */
  public String sizingMode = JobConfig.SIZING_FIXED;
  /** When true, blockSize is derived from image size ≈ √(pixels / targetStudCount). */
  public boolean autoSizing;
  /** Target stud count for auto sizing; unused when autoSizing is false. */
  public int targetStudCount;
  /** Target packed piece count for pieces sizing; unused otherwise. */
  public int targetPieceCount;
  /** How many DLX probes ran during piece search (0 if not used). */
  public int pieceSearchAttempts;
  /** Best probe piece count from search (final pipeline may differ slightly). */
  public int achievedPieceCount;
  /** When true, also emit a 1×1 stud parts list (every stud as its own plate). */
  public boolean includeStudBom;
  public int renderStudSizePx;
  public List<String> modes = new ArrayList<>();
  /** Logical artifact name -> filename inside the job directory. */
  public Map<String, String> artifacts = new LinkedHashMap<>();
  public PipelineResult result;

  public JobStatus statusEnum() {
    return JobStatus.valueOf(status);
  }

  /** Effective sizing mode, including legacy {@code autoSizing}-only manifests. */
  public String effectiveSizingMode() {
    if (sizingMode != null && !sizingMode.isBlank()) {
      return sizingMode;
    }
    return autoSizing ? JobConfig.SIZING_AUTO : JobConfig.SIZING_FIXED;
  }

  public boolean isPieceSizing() {
    return JobConfig.SIZING_PIECES.equals(effectiveSizingMode());
  }

  public boolean isAutoStudSizing() {
    return JobConfig.SIZING_AUTO.equals(effectiveSizingMode()) || autoSizing;
  }
}

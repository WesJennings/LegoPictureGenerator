package com.legopicturegenerator.domain;

import java.nio.file.Path;
import java.util.Set;

/** Everything one pipeline run needs. No global state, no fixed paths. */
public record JobConfig(
    Path inputPath,
    Path outputDirectory,
    int blockSize,
    /** When &gt; 0, sample with {@code toStudGrid(width)} instead of block size. */
    int targetStudWidth,
    int renderStudSizePx,
    Set<PackMode> modes,
    boolean includeStudBom) {

  /** Block-size sampling, no stud BOM (CLI / tests). */
  public JobConfig(
      Path inputPath,
      Path outputDirectory,
      int blockSize,
      int renderStudSizePx,
      Set<PackMode> modes) {
    this(inputPath, outputDirectory, blockSize, 0, renderStudSizePx, modes, false);
  }

  /** Same fixed pixel block size the old CLI used (`BLOCK_SIZE = 80`). */
  public static final int DEFAULT_BLOCK_SIZE = 80;
  public static final int MIN_BLOCK_SIZE = 8;
  public static final int MAX_BLOCK_SIZE = 256;
  public static final int DEFAULT_RENDER_STUD_PX = 24;

  /**
   * Classic web sizing: aspect-preserving mosaic ~54 studs wide
   * (matches historical Jarvis-at-B=80 on large photos; works for small uploads too).
   */
  public static final int CLASSIC_STUD_WIDTH = 54;

  /** Rough mosaic size when auto-sizing: B ≈ √(pixels / targetStuds). */
  public static final int DEFAULT_TARGET_STUDS = 4000;
  public static final int MIN_TARGET_STUDS = 400;
  public static final int MAX_TARGET_STUDS = 12_000;

  /** Piece-target search (DLX probes). See docs/sizing/MATH.md. */
  public static final int DEFAULT_TARGET_PIECES = 800;
  public static final int MIN_TARGET_PIECES = 150;
  public static final int MAX_TARGET_PIECES = 4000;
  public static final double PIECE_SEARCH_RATIO = 0.28;
  public static final double PIECE_SEARCH_TOLERANCE = 0.10;
  public static final int MAX_PIECE_PROBES = 5;

  public static final String SIZING_FIXED = "fixed";
  public static final String SIZING_AUTO = "auto";
  public static final String SIZING_PIECES = "pieces";

  public JobConfig {
    if (targetStudWidth > 0) {
      if (targetStudWidth < 1 || targetStudWidth > 512) {
        throw new IllegalArgumentException("targetStudWidth must be in [1, 512]");
      }
    } else if (blockSize < MIN_BLOCK_SIZE || blockSize > MAX_BLOCK_SIZE) {
      throw new IllegalArgumentException(
          "blockSize must be in [" + MIN_BLOCK_SIZE + ", " + MAX_BLOCK_SIZE + "]");
    }
    if (renderStudSizePx < 4) {
      throw new IllegalArgumentException("renderStudSizePx must be >= 4");
    }
    if (modes == null || modes.isEmpty()) {
      throw new IllegalArgumentException("at least one pack mode is required");
    }
  }

  public boolean usesStudWidthSampling() {
    return targetStudWidth > 0;
  }
}

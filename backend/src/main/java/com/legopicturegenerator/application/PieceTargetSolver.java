package com.legopicturegenerator.application;

import com.legopicturegenerator.core.image.ImageSampler;
import com.legopicturegenerator.domain.JobConfig;

/**
 * Binary-searches a stud-count target so a packing probe lands near a desired
 * piece count. See {@code docs/sizing/MATH.md}.
 */
public final class PieceTargetSolver {

  private PieceTargetSolver() {}

  /** One packing probe for a chosen block size. */
  @FunctionalInterface
  public interface Probe {
    int pieceCount(int blockSize) throws Exception;
  }

  public record Result(
      int blockSize,
      int studTargetUsed,
      int bestPieceCount,
      int attempts) {}

  /**
   * Search for a block size whose DLX (or other) pack piece count is near
   * {@code targetPieces}. Keeps the closest probe; stops early within tolerance.
   */
  public static Result solve(
      int srcWidth,
      int srcHeight,
      int targetPieces,
      Probe probe)
      throws Exception {
    if (targetPieces < 1) {
      throw new IllegalArgumentException("targetPieces must be >= 1");
    }

    int lo = JobConfig.MIN_TARGET_STUDS;
    int hi = JobConfig.MAX_TARGET_STUDS;
    int mid = clamp(
        (int) Math.round(targetPieces / JobConfig.PIECE_SEARCH_RATIO), lo, hi);

    int bestBlock = ImageSampler.blockSizeForTargetStuds(srcWidth, srcHeight, mid);
    int bestPieces = -1;
    int bestErr = Integer.MAX_VALUE;
    int bestStudTarget = mid;
    int attempts = 0;

    for (int i = 0; i < JobConfig.MAX_PIECE_PROBES && lo <= hi; i++) {
      attempts++;
      int blockSize = ImageSampler.blockSizeForTargetStuds(srcWidth, srcHeight, mid);
      int pieces = probe.pieceCount(blockSize);
      int err = Math.abs(pieces - targetPieces);
      if (err < bestErr || (err == bestErr && (bestPieces < 0 || pieces < bestPieces))) {
        bestErr = err;
        bestPieces = pieces;
        bestBlock = blockSize;
        bestStudTarget = mid;
      }

      double rel = targetPieces == 0 ? 0 : (double) err / targetPieces;
      if (rel <= JobConfig.PIECE_SEARCH_TOLERANCE) {
        break;
      }

      if (pieces > targetPieces) {
        // Too many pieces → shrink grid (fewer studs).
        hi = mid - 1;
      } else {
        lo = mid + 1;
      }
      if (lo > hi) {
        break;
      }
      mid = (lo + hi) / 2;
    }

    return new Result(bestBlock, bestStudTarget, bestPieces, attempts);
  }

  private static int clamp(int value, int min, int max) {
    return Math.max(min, Math.min(max, value));
  }
}

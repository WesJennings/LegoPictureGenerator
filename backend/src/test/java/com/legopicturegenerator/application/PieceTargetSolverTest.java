package com.legopicturegenerator.application;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.legopicturegenerator.core.image.ImageSampler;
import com.legopicturegenerator.domain.JobConfig;
import org.junit.jupiter.api.Test;

class PieceTargetSolverTest {

  /** Monotonic fake: piece count tracks stud count at a fixed ratio. */
  @Test
  void convergesNearTargetWithMonotonicProbe() throws Exception {
    int srcW = 4000;
    int srcH = 3000;
    int targetPieces = 800;
    double ratio = 0.28;

    PieceTargetSolver.Result result = PieceTargetSolver.solve(
        srcW,
        srcH,
        targetPieces,
        blockSize -> {
          int studs = ImageSampler.studCountFor(srcW, srcH, blockSize);
          return Math.max(1, (int) Math.round(studs * ratio));
        });

    assertTrue(result.attempts() >= 1);
    assertTrue(result.attempts() <= JobConfig.MAX_PIECE_PROBES);
    double rel = Math.abs(result.bestPieceCount() - targetPieces) / (double) targetPieces;
    assertTrue(rel <= JobConfig.PIECE_SEARCH_TOLERANCE + 0.02,
        "relative error " + rel + " pieces=" + result.bestPieceCount());
    assertTrue(result.blockSize() >= JobConfig.MIN_BLOCK_SIZE);
    assertTrue(result.blockSize() <= JobConfig.MAX_BLOCK_SIZE);
  }

  @Test
  void keepsClosestWhenNeverWithinTolerance() throws Exception {
    // Probe always returns the same piece count far from target.
    PieceTargetSolver.Result result = PieceTargetSolver.solve(
        800, 600, 500, blockSize -> 2000);

    assertEquals(2000, result.bestPieceCount());
    assertTrue(result.attempts() >= 1);
  }
}

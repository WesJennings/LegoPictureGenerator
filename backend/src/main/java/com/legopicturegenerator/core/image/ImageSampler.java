package com.legopicturegenerator.core.image;

import com.legopicturegenerator.domain.JobConfig;
import java.awt.image.BufferedImage;

/**
 * Converts an arbitrary image into a stud grid.
 *
 * <p>Default path matches the old CLI: each {@code blockSize × blockSize}
 * pixel block (historically {@code BLOCK_SIZE = 80}) becomes one stud.
 * A target-width path remains for tests and later UI controls.
 */
public final class ImageSampler {

  private ImageSampler() {}

  /** Stud count for a given source size and block size. */
  public static int studCountFor(int srcWidth, int srcHeight, int blockSize) {
    if (blockSize < 1) {
      throw new IllegalArgumentException("blockSize must be >= 1");
    }
    int tw = Math.max(1, srcWidth / blockSize);
    int th = Math.max(1, srcHeight / blockSize);
    return tw * th;
  }

  /**
   * Pick a block size so {@code floor(w/B)×floor(h/B)} is near {@code targetStuds}.
   * Uses {@code B ≈ √(pixels / targetStuds)}, then checks nearby B values.
   */
  public static int blockSizeForTargetStuds(int srcWidth, int srcHeight, int targetStuds) {
    if (srcWidth < 1 || srcHeight < 1) {
      throw new IllegalArgumentException("image dimensions must be positive");
    }
    if (targetStuds < 1) {
      throw new IllegalArgumentException("targetStuds must be >= 1");
    }
    long pixels = (long) srcWidth * srcHeight;
    int guess = (int) Math.round(Math.sqrt((double) pixels / targetStuds));
    guess = clampBlockSize(guess);

    int best = guess;
    long bestErr = Long.MAX_VALUE;
    for (int b = Math.max(JobConfig.MIN_BLOCK_SIZE, guess - 4);
         b <= Math.min(JobConfig.MAX_BLOCK_SIZE, guess + 4);
         b++) {
      long err = Math.abs((long) studCountFor(srcWidth, srcHeight, b) - targetStuds);
      if (err < bestErr || (err == bestErr && b < best)) {
        bestErr = err;
        best = b;
      }
    }
    return best;
  }

  private static int clampBlockSize(int blockSize) {
    if (blockSize < JobConfig.MIN_BLOCK_SIZE) {
      return JobConfig.MIN_BLOCK_SIZE;
    }
    if (blockSize > JobConfig.MAX_BLOCK_SIZE) {
      return JobConfig.MAX_BLOCK_SIZE;
    }
    return blockSize;
  }

  /**
   * Old-style downscale: grid size is {@code floor(w/blockSize) × floor(h/blockSize)}.
   * Remainder pixels on the right/bottom edges are dropped (same as before).
   */
  public static BufferedImage toStudGridByBlockSize(BufferedImage src, int blockSize) {
    if (blockSize < 1) {
      throw new IllegalArgumentException("blockSize must be >= 1");
    }
    int sw = src.getWidth();
    int sh = src.getHeight();
    int tw = Math.max(1, sw / blockSize);
    int th = Math.max(1, sh / blockSize);
    return averageBlocks(src, tw, th, blockSize, blockSize);
  }

  /** Derived grid height for a target width, preserving aspect ratio (min 1). */
  public static int gridHeightFor(int srcWidth, int srcHeight, int targetStudWidth) {
    return Math.max(1, Math.round((float) srcHeight * targetStudWidth / srcWidth));
  }

  /**
   * Box-average the source into a {@code targetStudWidth}-wide grid.
   * Each output pixel is the mean RGB of its (proportional) source block.
   */
  public static BufferedImage toStudGrid(BufferedImage src, int targetStudWidth) {
    int sw = src.getWidth();
    int sh = src.getHeight();
    int tw = Math.min(targetStudWidth, sw);
    int th = Math.min(gridHeightFor(sw, sh, targetStudWidth), sh);
    if (tw < 1) {
      tw = 1;
    }
    if (th < 1) {
      th = 1;
    }

    int[] pixels = src.getRGB(0, 0, sw, sh, null, 0, sw);
    BufferedImage out = new BufferedImage(tw, th, BufferedImage.TYPE_INT_ARGB);

    for (int oy = 0; oy < th; oy++) {
      int y0 = (int) ((long) oy * sh / th);
      int y1 = Math.max(y0 + 1, (int) ((long) (oy + 1) * sh / th));
      for (int ox = 0; ox < tw; ox++) {
        int x0 = (int) ((long) ox * sw / tw);
        int x1 = Math.max(x0 + 1, (int) ((long) (ox + 1) * sw / tw));
        out.setRGB(ox, oy, averageRegion(pixels, sw, x0, x1, y0, y1));
      }
    }
    return out;
  }

  private static BufferedImage averageBlocks(
      BufferedImage src, int tw, int th, int blockW, int blockH) {
    int sw = src.getWidth();
    int[] pixels = src.getRGB(0, 0, sw, src.getHeight(), null, 0, sw);
    BufferedImage out = new BufferedImage(tw, th, BufferedImage.TYPE_INT_ARGB);
    for (int oy = 0; oy < th; oy++) {
      int y0 = oy * blockH;
      int y1 = y0 + blockH;
      for (int ox = 0; ox < tw; ox++) {
        int x0 = ox * blockW;
        int x1 = x0 + blockW;
        out.setRGB(ox, oy, averageRegion(pixels, sw, x0, x1, y0, y1));
      }
    }
    return out;
  }

  private static int averageRegion(
      int[] pixels, int sw, int x0, int x1, int y0, int y1) {
    long sumR = 0;
    long sumG = 0;
    long sumB = 0;
    int count = (y1 - y0) * (x1 - x0);
    for (int y = y0; y < y1; y++) {
      int row = y * sw;
      for (int x = x0; x < x1; x++) {
        int p = pixels[row + x];
        sumR += (p >> 16) & 0xFF;
        sumG += (p >> 8) & 0xFF;
        sumB += p & 0xFF;
      }
    }
    return (0xFF << 24)
        | ((int) (sumR / count) << 16)
        | ((int) (sumG / count) << 8)
        | (int) (sumB / count);
  }
}

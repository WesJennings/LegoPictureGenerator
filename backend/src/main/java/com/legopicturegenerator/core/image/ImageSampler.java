package com.legopicturegenerator.core.image;

import com.legopicturegenerator.core.nativeengine.NativeEngine;
import com.legopicturegenerator.domain.JobConfig;
import java.awt.image.BufferedImage;

/**
 * Converts an arbitrary image into a stud grid.
 *
 * <p>Pixel averaging runs in the C++ engine ({@link NativeEngine}); Java keeps
 * the BufferedImage adapters. Logic matches the previous Java implementation
 * (integer box average, remainder pixels dropped on the block-size path).
 */
public final class ImageSampler {

  private ImageSampler() {}

  /** Stud count for a given source size and block size. */
  public static int studCountFor(int srcWidth, int srcHeight, int blockSize) {
    NativeEngine.ensureLoaded();
    if (blockSize < 1) {
      throw new IllegalArgumentException("blockSize must be >= 1");
    }
    return NativeEngine.studCountFor(srcWidth, srcHeight, blockSize);
  }

  /**
   * Pick a block size so {@code floor(w/B)×floor(h/B)} is near {@code targetStuds}.
   * Uses {@code B ≈ √(pixels / targetStuds)}, then checks nearby B values.
   */
  public static int blockSizeForTargetStuds(int srcWidth, int srcHeight, int targetStuds) {
    NativeEngine.ensureLoaded();
    return NativeEngine.blockSizeForTargetStuds(srcWidth, srcHeight, targetStuds);
  }

  /**
   * Old-style downscale: grid size is {@code floor(w/blockSize) × floor(h/blockSize)}.
   * Remainder pixels on the right/bottom edges are dropped (same as before).
   */
  public static BufferedImage toStudGridByBlockSize(BufferedImage src, int blockSize) {
    NativeEngine.ensureLoaded();
    if (blockSize < 1) {
      throw new IllegalArgumentException("blockSize must be >= 1");
    }
    int sw = src.getWidth();
    int sh = src.getHeight();
    int[] pixels = src.getRGB(0, 0, sw, sh, null, 0, sw);
    int[] wh = new int[2];
    int[] out = NativeEngine.sampleByBlockSize(pixels, sw, sh, blockSize, wh);
    return toImage(out, wh[0], wh[1]);
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
    NativeEngine.ensureLoaded();
    int sw = src.getWidth();
    int sh = src.getHeight();
    int[] pixels = src.getRGB(0, 0, sw, sh, null, 0, sw);
    int[] wh = new int[2];
    int[] out = NativeEngine.sampleByWidth(pixels, sw, sh, targetStudWidth, wh);
    return toImage(out, wh[0], wh[1]);
  }

  private static BufferedImage toImage(int[] argb, int w, int h) {
    BufferedImage img = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB);
    img.setRGB(0, 0, w, h, argb, 0, w);
    return img;
  }

  // Retain JobConfig bounds in docs/callers; native constants must match these.
  @SuppressWarnings("unused")
  private static final int MIN_BLOCK_SIZE = JobConfig.MIN_BLOCK_SIZE;
}

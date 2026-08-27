package com.legopicturegenerator.core.image;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.nativeengine.NativeEngine;
import com.legopicturegenerator.core.pack.PackResult;
import com.legopicturegenerator.core.pack.PlacedPart;
import java.awt.image.BufferedImage;
import java.util.List;

/**
 * Procedural 2D LEGO-look transform. Pixel drawing runs in C++ (same plate
 * geometry and shade factors; knobs use a software ellipse rasterizer instead
 * of Java2D antialiasing).
 */
public class LegoRenderer {
  public static final int DEFAULT_STUD_SIZE_PX = 24;

  public static BufferedImage renderStuds(BufferedImage studColors, int studSizePx) {
    NativeEngine.ensureLoaded();
    if (studSizePx < 4) {
      throw new IllegalArgumentException("studSizePx must be >= 4");
    }
    int cols = studColors.getWidth();
    int rows = studColors.getHeight();
    int[] grid = studColors.getRGB(0, 0, cols, rows, null, 0, cols);
    int[] out = NativeEngine.renderStuds(grid, cols, rows, studSizePx);
    return toImage(out, cols * studSizePx, rows * studSizePx);
  }

  public static BufferedImage renderPacked(
      List<PlacedPart> placed,
      ColorMatcher.LegoElement[][] studs,
      int studSizePx) {
    NativeEngine.ensureLoaded();
    if (studSizePx < 4) {
      throw new IllegalArgumentException("studSizePx must be >= 4");
    }
    if (studs == null || studs.length == 0 || studs[0].length == 0) {
      throw new IllegalArgumentException("studs grid must be non-empty");
    }
    if (placed == null) {
      throw new IllegalArgumentException("placed must not be null");
    }
    int rows = studs.length;
    int cols = studs[0].length;
    int[] grid = NativeEngine.gridArgb(studs);
    int[][] xywh = NativeEngine.unpackCoords(placed);
    int[] out = NativeEngine.renderPacked(
        grid, cols, rows, studSizePx, xywh[0], xywh[1], xywh[2], xywh[3]);
    return toImage(out, cols * studSizePx, rows * studSizePx);
  }

  public static BufferedImage renderPacked(
      PackResult result,
      ColorMatcher.LegoElement[][] studs,
      int studSizePx) {
    if (result == null) {
      throw new IllegalArgumentException("result must not be null");
    }
    return renderPacked(result.placed, studs, studSizePx);
  }

  private static BufferedImage toImage(int[] argb, int w, int h) {
    BufferedImage img = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB);
    img.setRGB(0, 0, w, h, argb, 0, w);
    return img;
  }
}

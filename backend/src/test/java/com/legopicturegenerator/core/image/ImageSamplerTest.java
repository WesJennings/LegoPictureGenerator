package com.legopicturegenerator.core.image;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.awt.image.BufferedImage;
import org.junit.jupiter.api.Test;

class ImageSamplerTest {

  private static BufferedImage solid(int w, int h, int rgb) {
    BufferedImage img = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        img.setRGB(x, y, 0xFF000000 | rgb);
      }
    }
    return img;
  }

  @Test
  void blockSizeMatchesLegacyDownscale() {
    // 160×80 with BLOCK_SIZE 80 → 2×1 studs
    BufferedImage out = ImageSampler.toStudGridByBlockSize(solid(160, 80, 0x123456), 80);
    assertEquals(2, out.getWidth());
    assertEquals(1, out.getHeight());
  }

  @Test
  void autoBlockSizeNearTargetStudCount() {
    // 4000×3000 → 12e6 pixels; target 4000 studs → B ≈ √3000 ≈ 55
    int b = ImageSampler.blockSizeForTargetStuds(4000, 3000, 4000);
    int studs = ImageSampler.studCountFor(4000, 3000, b);
    assertTrue(b >= 8 && b <= 256);
    // Roughly close — within ~25% of the goal is fine for this heuristic
    assertTrue(Math.abs(studs - 4000) < 1000, "studs=" + studs + " block=" + b);
  }

  @Test
  void autoBlockSizeClampsToRange() {
    // Tiny image + huge target → smallest block size
    int b = ImageSampler.blockSizeForTargetStuds(100, 100, 12_000);
    assertEquals(8, b);
  }

  @Test
  void preservesAspectRatio() {
    BufferedImage out = ImageSampler.toStudGrid(solid(400, 200, 0x123456), 40);
    assertEquals(40, out.getWidth());
    assertEquals(20, out.getHeight());
  }

  @Test
  void neverProducesZeroHeight() {
    // Extremely wide strip: height must clamp to at least 1
    BufferedImage out = ImageSampler.toStudGrid(solid(1000, 3, 0xABCDEF), 54);
    assertEquals(54, out.getWidth());
    assertTrue(out.getHeight() >= 1);
  }

  @Test
  void sourceSmallerThanTargetClampsToSource() {
    BufferedImage out = ImageSampler.toStudGrid(solid(10, 10, 0xFF0000), 54);
    assertEquals(10, out.getWidth());
    assertEquals(10, out.getHeight());
  }

  @Test
  void nonDivisibleDimensionsCoverAllPixels() {
    // 7x5 source into 3-wide grid — every output cell must still be the average
    BufferedImage src = solid(7, 5, 0x808080);
    BufferedImage out = ImageSampler.toStudGrid(src, 3);
    assertEquals(3, out.getWidth());
    for (int y = 0; y < out.getHeight(); y++) {
      for (int x = 0; x < out.getWidth(); x++) {
        assertEquals(0xFF808080, out.getRGB(x, y));
      }
    }
  }

  @Test
  void averagesBlockColors() {
    // Left half black, right half white, 2-wide target → one dark, one light cell
    BufferedImage src = new BufferedImage(20, 10, BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < 10; y++) {
      for (int x = 0; x < 20; x++) {
        src.setRGB(x, y, x < 10 ? 0xFF000000 : 0xFFFFFFFF);
      }
    }
    BufferedImage out = ImageSampler.toStudGrid(src, 2);
    assertEquals(0xFF000000, out.getRGB(0, 0));
    assertEquals(0xFFFFFFFF, out.getRGB(1, 0));
  }
}

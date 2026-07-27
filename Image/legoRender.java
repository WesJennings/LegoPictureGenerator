import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.util.List;

/**
 * Procedural 2D LEGO-look transform. Pure functions — no file I/O.
 * Can render a flat per-stud mosaic or a packed {@link PlacedPart} list.
 */
public class legoRender {
  public static final int DEFAULT_STUD_SIZE_PX = 24;

  /**
   * Upscale a flat one-pixel-per-stud image into plates with knobs
   * (every stud is its own 1×1 visual plate).
   *
   * @param studColors flat matched mosaic (each pixel = one stud color)
   * @param studSizePx pixels per stud cell on the output (e.g. 24)
   */
  public static BufferedImage renderStuds(BufferedImage studColors, int studSizePx) {
    if (studSizePx < 4) {
      throw new IllegalArgumentException("studSizePx must be >= 4");
    }

    int cols = studColors.getWidth();
    int rows = studColors.getHeight();
    BufferedImage out = newCanvas(cols, rows, studSizePx);
    Graphics2D g = createGraphics(out);

    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        drawPlate(g, x * studSizePx, y * studSizePx, 1, 1, studSizePx, studColors.getRGB(x, y));
      }
    }

    g.dispose();
    return out;
  }

  /**
   * Render a packed placement list as multi-stud plates with knobs.
   * Plate colors are sampled from {@code studs} at each part's origin.
   * Grid size comes from {@code studs} dimensions; uncovered cells stay blank.
   *
   * @param placed     placements from a packer (fast or exact)
   * @param studs      matched stud grid (read-only; used for size + RGB)
   * @param studSizePx pixels per stud cell on the output (e.g. 24)
   */
  public static BufferedImage renderPacked(
      List<PlacedPart> placed,
      colorMatch.LegoElement[][] studs,
      int studSizePx) {
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
    BufferedImage out = newCanvas(cols, rows, studSizePx);
    Graphics2D g = createGraphics(out);

    // Neutral backdrop so gaps (if any) are visible
    g.setColor(new Color(0x2A2A2A));
    g.fillRect(0, 0, cols * studSizePx, rows * studSizePx);

    for (PlacedPart p : placed) {
      if (p == null || p.w <= 0 || p.h <= 0) {
        continue;
      }
      if (p.y < 0 || p.x < 0 || p.y >= rows || p.x >= cols) {
        continue;
      }
      colorMatch.LegoElement el = studs[p.y][p.x];
      if (el == null) {
        continue;
      }
      drawPlate(
          g,
          p.x * studSizePx,
          p.y * studSizePx,
          p.w,
          p.h,
          studSizePx,
          el.toArgb());
    }

    g.dispose();
    return out;
  }

  /** Convenience: render a full {@link PackResult}. */
  public static BufferedImage renderPacked(
      PackResult result,
      colorMatch.LegoElement[][] studs,
      int studSizePx) {
    if (result == null) {
      throw new IllegalArgumentException("result must not be null");
    }
    return renderPacked(result.placed, studs, studSizePx);
  }

  private static BufferedImage newCanvas(int cols, int rows, int studSizePx) {
    return new BufferedImage(
        cols * studSizePx,
        rows * studSizePx,
        BufferedImage.TYPE_INT_ARGB);
  }

  private static Graphics2D createGraphics(BufferedImage out) {
    Graphics2D g = out.createGraphics();
    g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
    return g;
  }

  /**
   * Draw one physical plate covering {@code studsW × studsH} studs.
   * Body is continuous; knobs sit on each stud; border marks the plate edge.
   */
  private static void drawPlate(
      Graphics2D g,
      int ox,
      int oy,
      int studsW,
      int studsH,
      int studSize,
      int argb) {
    Color base = new Color(argb, true);
    Color edge = shade(base, 0.62f);
    Color rim = shade(base, 0.88f);
    int pw = studsW * studSize;
    int ph = studsH * studSize;

    // Continuous plate plastic
    g.setColor(base);
    g.fillRect(ox, oy, pw, ph);

    // Soft inner rim (top/left lighter feel)
    g.setColor(rim);
    g.fillRect(ox, oy, pw, 1);
    g.fillRect(ox, oy, 1, ph);

    // Outer plate edge (right/bottom) — shows multi-stud plate boundaries
    g.setColor(edge);
    g.fillRect(ox + pw - 2, oy, 2, ph);
    g.fillRect(ox, oy + ph - 2, pw, 2);

    // Knobs on every stud of this plate
    for (int sy = 0; sy < studsH; sy++) {
      for (int sx = 0; sx < studsW; sx++) {
        drawKnob(g, ox + sx * studSize, oy + sy * studSize, studSize, base);
      }
    }
  }

  private static void drawKnob(Graphics2D g, int ox, int oy, int size, Color base) {
    Color knob = shade(base, 1.18f);
    Color knobShadow = shade(base, 0.85f);
    Color knobHighlight = shade(base, 1.35f);

    int knobSize = Math.max(4, (int) (size * 0.60));
    int kx = ox + (size - knobSize) / 2;
    int ky = oy + (size - knobSize) / 2;

    g.setColor(knob);
    g.fillOval(kx, ky, knobSize, knobSize);

    g.setColor(knobShadow);
    g.fillArc(kx, ky + knobSize / 3, knobSize, (knobSize * 2) / 3, 200, 140);

    int hi = Math.max(2, knobSize / 3);
    g.setColor(knobHighlight);
    g.fillOval(kx + knobSize / 4, ky + knobSize / 6, hi, hi);
  }

  /** Multiply RGB by factor and clamp; keep original alpha. */
  private static Color shade(Color c, float factor) {
    int r = clamp((int) (c.getRed() * factor));
    int g = clamp((int) (c.getGreen() * factor));
    int b = clamp((int) (c.getBlue() * factor));
    return new Color(r, g, b, c.getAlpha());
  }

  private static int clamp(int v) {
    return Math.max(0, Math.min(255, v));
  }
}

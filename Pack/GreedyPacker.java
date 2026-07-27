import java.util.ArrayList;
import java.util.List;

/**
 * Greedy largest-first packing with multi-order repair.
 * Future: bias sizes toward cheapest/most common parts.
 */
public class GreedyPacker {
  private final PlateCatalog catalog;

  public GreedyPacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(colorMatch.LegoElement[][] studs) {
    long t0 = System.currentTimeMillis();
    if (studs.length == 0 || studs[0].length == 0) {
      return new PackResult("greedy", new ArrayList<>(), 0, "ok");
    }

    int h = studs.length;
    int w = studs[0].length;

    List<PlacedPart> best = null;
    // Several scan orders; keep fewest pieces.
    List<List<PlacedPart>> candidates = new ArrayList<>();
    candidates.add(packRowMajor(studs, w, h, true, true));
    candidates.add(packRowMajor(studs, w, h, true, false));
    candidates.add(packRowMajor(studs, w, h, false, true));
    candidates.add(packColMajor(studs, w, h));

    for (List<PlacedPart> candidate : candidates) {
      List<PlacedPart> repaired = repairLeftovers(studs, w, h, candidate);
      if (best == null || repaired.size() < best.size()) {
        best = repaired;
      }
    }

    long ms = System.currentTimeMillis() - t0;
    return new PackResult("greedy", best, ms, "ok");
  }

  private List<PlacedPart> packRowMajor(
      colorMatch.LegoElement[][] studs,
      int w,
      int h,
      boolean topToBottom,
      boolean leftToRight) {
    boolean[][] covered = new boolean[h][w];
    List<PlacedPart> placed = new ArrayList<>();
    int y0 = topToBottom ? 0 : h - 1;
    int y1 = topToBottom ? h : -1;
    int ys = topToBottom ? 1 : -1;
    int x0 = leftToRight ? 0 : w - 1;
    int x1 = leftToRight ? w : -1;
    int xs = leftToRight ? 1 : -1;
    for (int y = y0; y != y1; y += ys) {
      for (int x = x0; x != x1; x += xs) {
        placeAt(studs, covered, placed, x, y, w, h);
      }
    }
    return placed;
  }

  private List<PlacedPart> packColMajor(
      colorMatch.LegoElement[][] studs, int w, int h) {
    boolean[][] covered = new boolean[h][w];
    List<PlacedPart> placed = new ArrayList<>();
    for (int x = 0; x < w; x++) {
      for (int y = 0; y < h; y++) {
        placeAt(studs, covered, placed, x, y, w, h);
      }
    }
    return placed;
  }

  private void placeAt(
      colorMatch.LegoElement[][] studs,
      boolean[][] covered,
      List<PlacedPart> placed,
      int x,
      int y,
      int w,
      int h) {
    if (covered[y][x]) {
      return;
    }
    colorMatch.LegoElement el = studs[y][x];
    if (el == null) {
      covered[y][x] = true;
      return;
    }
    int colorId = el.colorId;
    for (PlateCatalog.PlateSize size : catalog.footprintsForColor(colorId)) {
      if (fits(studs, covered, x, y, size.w, size.h, colorId, w, h)) {
        mark(covered, x, y, size.w, size.h);
        placed.add(new PlacedPart(
            size.partNum, colorId, el.colorName, x, y, size.w, size.h));
        return;
      }
    }
    covered[y][x] = true;
    placed.add(new PlacedPart("3024", colorId, el.colorName, x, y, 1, 1));
  }

  /** Re-pack cells that ended as 1x1 using a reverse scan order. */
  private List<PlacedPart> repairLeftovers(
      colorMatch.LegoElement[][] studs, int w, int h, List<PlacedPart> initial) {
    boolean[][] covered = new boolean[h][w];
    List<PlacedPart> repaired = new ArrayList<>();
    boolean[][] need = new boolean[h][w];

    for (PlacedPart p : initial) {
      if (p.w == 1 && p.h == 1) {
        need[p.y][p.x] = true;
      } else {
        repaired.add(p);
        mark(covered, p.x, p.y, p.w, p.h);
      }
    }

    for (int y = h - 1; y >= 0; y--) {
      for (int x = w - 1; x >= 0; x--) {
        if (need[y][x] && !covered[y][x]) {
          placeAt(studs, covered, repaired, x, y, w, h);
        }
      }
    }

    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        if (!covered[y][x] && studs[y][x] != null) {
          placeAt(studs, covered, repaired, x, y, w, h);
        }
      }
    }
    return repaired;
  }

  static boolean fits(
      colorMatch.LegoElement[][] studs,
      boolean[][] covered,
      int x,
      int y,
      int pw,
      int ph,
      int colorId,
      int w,
      int h) {
    if (x + pw > w || y + ph > h) {
      return false;
    }
    for (int dy = 0; dy < ph; dy++) {
      for (int dx = 0; dx < pw; dx++) {
        if (covered[y + dy][x + dx]) {
          return false;
        }
        colorMatch.LegoElement el = studs[y + dy][x + dx];
        if (el == null || el.colorId != colorId) {
          return false;
        }
      }
    }
    return true;
  }

  static void mark(boolean[][] covered, int x, int y, int pw, int ph) {
    for (int dy = 0; dy < ph; dy++) {
      for (int dx = 0; dx < pw; dx++) {
        covered[y + dy][x + dx] = true;
      }
    }
  }
}

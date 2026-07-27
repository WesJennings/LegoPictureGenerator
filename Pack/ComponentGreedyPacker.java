import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;

/**
 * Connected-component greedy packing (survey alg 3).
 * Flood-fill same-color blobs, then largest-first greedy per blob (no multi-order repair).
 */
public class ComponentGreedyPacker {
  private final PlateCatalog catalog;

  public ComponentGreedyPacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(colorMatch.LegoElement[][] studs) {
    long t0 = System.currentTimeMillis();
    if (studs.length == 0 || studs[0].length == 0) {
      return new PackResult("component", new ArrayList<>(), 0, "ok");
    }

    int h = studs.length;
    int w = studs[0].length;
    boolean[][] visited = new boolean[h][w];
    List<PlacedPart> all = new ArrayList<>();

    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        if (visited[y][x] || studs[y][x] == null) {
          continue;
        }
        int colorId = studs[y][x].colorId;
        List<int[]> cells = flood(studs, visited, x, y, colorId, w, h);
        all.addAll(greedyComponent(studs, cells, w, h));
      }
    }

    long ms = System.currentTimeMillis() - t0;
    return new PackResult("component", all, ms, "ok");
  }

  private static List<int[]> flood(
      colorMatch.LegoElement[][] studs,
      boolean[][] visited,
      int sx,
      int sy,
      int colorId,
      int w,
      int h) {
    List<int[]> cells = new ArrayList<>();
    ArrayDeque<int[]> q = new ArrayDeque<>();
    q.add(new int[] {sx, sy});
    visited[sy][sx] = true;
    while (!q.isEmpty()) {
      int[] c = q.removeFirst();
      cells.add(c);
      tryVisit(studs, visited, q, c[0] + 1, c[1], colorId, w, h);
      tryVisit(studs, visited, q, c[0] - 1, c[1], colorId, w, h);
      tryVisit(studs, visited, q, c[0], c[1] + 1, colorId, w, h);
      tryVisit(studs, visited, q, c[0], c[1] - 1, colorId, w, h);
    }
    return cells;
  }

  private static void tryVisit(
      colorMatch.LegoElement[][] studs,
      boolean[][] visited,
      ArrayDeque<int[]> q,
      int x,
      int y,
      int colorId,
      int w,
      int h) {
    if (x < 0 || y < 0 || x >= w || y >= h || visited[y][x]) {
      return;
    }
    colorMatch.LegoElement el = studs[y][x];
    if (el == null || el.colorId != colorId) {
      return;
    }
    visited[y][x] = true;
    q.add(new int[] {x, y});
  }

  private List<PlacedPart> greedyComponent(
      colorMatch.LegoElement[][] studs, List<int[]> cells, int w, int h) {
    boolean[][] inComp = new boolean[h][w];
    for (int[] c : cells) {
      inComp[c[1]][c[0]] = true;
    }
    boolean[][] covered = new boolean[h][w];
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        if (!inComp[y][x]) {
          covered[y][x] = true;
        }
      }
    }

    List<PlacedPart> placed = new ArrayList<>();
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        if (covered[y][x]) {
          continue;
        }
        colorMatch.LegoElement el = studs[y][x];
        int colorId = el.colorId;
        boolean placedOne = false;
        for (PlateCatalog.PlateSize size : catalog.footprintsForColor(colorId)) {
          if (!GreedyPacker.fits(studs, covered, x, y, size.w, size.h, colorId, w, h)) {
            continue;
          }
          boolean ok = true;
          for (int dy = 0; dy < size.h && ok; dy++) {
            for (int dx = 0; dx < size.w; dx++) {
              if (!inComp[y + dy][x + dx]) {
                ok = false;
                break;
              }
            }
          }
          if (!ok) {
            continue;
          }
          GreedyPacker.mark(covered, x, y, size.w, size.h);
          placed.add(new PlacedPart(
              size.partNum, colorId, el.colorName, x, y, size.w, size.h));
          placedOne = true;
          break;
        }
        if (!placedOne) {
          covered[y][x] = true;
          placed.add(new PlacedPart("3024", colorId, el.colorName, x, y, 1, 1));
        }
      }
    }
    return placed;
  }
}

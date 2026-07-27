import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Min-cardinality exact cover via Algorithm X–style search (survey alg 7).
 * Uses bitmask placements and the DLX “fewest options” column heuristic.
 * Large / timed-out blobs fall back to greedy (same budgets as ExactIlpPacker).
 */
public class DlxPacker {
  /** Max for a single {@code long} bitmask (bits 0..63). */
  private static final int EXACT_CELL_LIMIT = 64;
  private static final long COMPONENT_TIME_MS = 30_000;
  private static final long PACK_TIME_MS = 60_000;

  private final PlateCatalog catalog;

  public DlxPacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(colorMatch.LegoElement[][] studs) {
    long t0 = System.currentTimeMillis();
    long packDeadline = t0 + PACK_TIME_MS;
    if (studs.length == 0 || studs[0].length == 0) {
      return new PackResult("dlx", new ArrayList<>(), 0, "ok");
    }

    int h = studs.length;
    int w = studs[0].length;
    boolean[][] visited = new boolean[h][w];
    List<PlacedPart> all = new ArrayList<>();
    int exactComponents = 0;
    int fallbackComponents = 0;

    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        if (visited[y][x] || studs[y][x] == null) {
          continue;
        }
        int colorId = studs[y][x].colorId;
        List<int[]> cells = flood(studs, visited, x, y, colorId, w, h);
        long budget = Math.min(
            COMPONENT_TIME_MS,
            Math.max(0L, packDeadline - System.currentTimeMillis()));
        ComponentResult cr = packComponent(studs, cells, colorId, w, h, budget);
        all.addAll(cr.placed);
        if (cr.exact) {
          exactComponents++;
        } else {
          fallbackComponents++;
        }
      }
    }

    long ms = System.currentTimeMillis() - t0;
    String status = fallbackComponents == 0
        ? "optimal"
        : "exact_partial (" + exactComponents + " exact blobs, "
            + fallbackComponents + " greedy fallback)";
    return new PackResult("dlx", all, ms, status);
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

  private ComponentResult packComponent(
      colorMatch.LegoElement[][] studs,
      List<int[]> cells,
      int colorId,
      int w,
      int h,
      long timeBudgetMs) {
    if (cells.isEmpty()) {
      return new ComponentResult(new ArrayList<>(), true);
    }
    String colorName = studs[cells.get(0)[1]][cells.get(0)[0]].colorName;

    if (cells.size() > EXACT_CELL_LIMIT || timeBudgetMs <= 0) {
      return new ComponentResult(greedyComponent(studs, cells, w, h), false);
    }

    Map<Long, Integer> indexOf = new HashMap<>();
    for (int i = 0; i < cells.size(); i++) {
      int[] c = cells.get(i);
      indexOf.put(key(c[0], c[1]), i);
    }
    int n = cells.size();
    List<PlateCatalog.PlateSize> sizes = catalog.footprintsForColor(colorId);
    List<Placement> placements = new ArrayList<>();

    for (int[] cell : cells) {
      int x = cell[0];
      int y = cell[1];
      for (PlateCatalog.PlateSize size : sizes) {
        if (x + size.w > w || y + size.h > h) {
          continue;
        }
        long mask = 0L;
        boolean ok = true;
        for (int dy = 0; dy < size.h && ok; dy++) {
          for (int dx = 0; dx < size.w; dx++) {
            Integer idx = indexOf.get(key(x + dx, y + dy));
            if (idx == null) {
              ok = false;
              break;
            }
            mask |= (1L << idx);
          }
        }
        if (ok && Long.bitCount(mask) == size.area()) {
          placements.add(new Placement(size, x, y, mask, colorId, colorName));
        }
      }
    }

    for (int i = 0; i < n; i++) {
      int[] c = cells.get(i);
      placements.add(new Placement(
          new PlateCatalog.PlateSize("3024", 1, 1),
          c[0], c[1], 1L << i, colorId, colorName));
    }

    // Prefer larger pieces when branching among equal columns
    placements.sort((a, b) -> Integer.compare(b.size.area(), a.size.area()));

    SearchState ss = new SearchState();
    ss.bestPieces = n + 1;
    ss.deadline = System.currentTimeMillis() + timeBudgetMs;
    long full = (n == 64) ? -1L : ((1L << n) - 1L);
    search(0L, full, 0, placements, n, ss);

    if (ss.bestSolution == null) {
      return new ComponentResult(greedyComponent(studs, cells, w, h), false);
    }
    List<PlacedPart> out = new ArrayList<>();
    for (Placement p : ss.bestSolution) {
      out.add(new PlacedPart(
          p.size.partNum, p.colorId, p.colorName, p.x, p.y, p.size.w, p.size.h));
    }
    return new ComponentResult(out, !ss.timedOut);
  }

  private void search(
      long covered,
      long full,
      int pieces,
      List<Placement> placements,
      int n,
      SearchState ss) {
    if (System.currentTimeMillis() > ss.deadline) {
      ss.timedOut = true;
      return;
    }
    if (pieces >= ss.bestPieces) {
      return;
    }
    if (covered == full) {
      ss.bestPieces = pieces;
      ss.bestSolution = new ArrayList<>(ss.current);
      return;
    }

    int remaining = Long.bitCount(full & ~covered);
    int maxArea = 1;
    for (Placement p : placements) {
      if ((p.mask & covered) == 0) {
        maxArea = Math.max(maxArea, p.size.area());
      }
    }
    if (pieces + (remaining + maxArea - 1) / maxArea >= ss.bestPieces) {
      return;
    }

    // DLX heuristic: uncovered cell covered by the fewest remaining placements
    int bestBit = -1;
    int bestCount = Integer.MAX_VALUE;
    long uncovered = full & ~covered;
    for (int bit = 0; bit < n; bit++) {
      if (((uncovered >> bit) & 1L) == 0) {
        continue;
      }
      int count = 0;
      for (Placement p : placements) {
        if (((p.mask >> bit) & 1L) == 0) {
          continue;
        }
        if ((p.mask & covered) != 0) {
          continue;
        }
        count++;
      }
      if (count < bestCount) {
        bestCount = count;
        bestBit = bit;
        if (count == 0) {
          return; // impossible
        }
      }
    }
    if (bestBit < 0) {
      return;
    }

    for (Placement p : placements) {
      if (((p.mask >> bestBit) & 1L) == 0) {
        continue;
      }
      if ((p.mask & covered) != 0) {
        continue;
      }
      ss.current.add(p);
      search(covered | p.mask, full, pieces + 1, placements, n, ss);
      ss.current.remove(ss.current.size() - 1);
      if (ss.timedOut) {
        return;
      }
    }
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

  private static long key(int x, int y) {
    return (((long) y) << 32) ^ (x & 0xffffffffL);
  }

  private static final class Placement {
    final PlateCatalog.PlateSize size;
    final int x;
    final int y;
    final long mask;
    final int colorId;
    final String colorName;

    Placement(
        PlateCatalog.PlateSize size,
        int x,
        int y,
        long mask,
        int colorId,
        String colorName) {
      this.size = size;
      this.x = x;
      this.y = y;
      this.mask = mask;
      this.colorId = colorId;
      this.colorName = colorName;
    }
  }

  private static final class SearchState {
    int bestPieces;
    List<Placement> bestSolution;
    List<Placement> current = new ArrayList<>();
    long deadline;
    boolean timedOut;
  }

  private static final class ComponentResult {
    final List<PlacedPart> placed;
    final boolean exact;

    ComponentResult(List<PlacedPart> placed, boolean exact) {
      this.placed = placed;
      this.exact = exact;
    }
  }
}

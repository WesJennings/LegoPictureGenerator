import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;

/**
 * Simulated annealing local search (survey alg 9).
 * Seeds from GreedyPacker, then re-packs random windows with shuffled size orders.
 */
public class AnnealPacker {
  private static final long TIME_BUDGET_MS = 20_000;
  private static final int MAX_ITERS = 4000;
  private static final int WINDOW = 8;

  private final PlateCatalog catalog;
  private final Random rng;

  public AnnealPacker(PlateCatalog catalog) {
    this(catalog, new Random(42));
  }

  public AnnealPacker(PlateCatalog catalog, Random rng) {
    this.catalog = catalog;
    this.rng = rng;
  }

  public PackResult pack(colorMatch.LegoElement[][] studs) {
    long t0 = System.currentTimeMillis();
    long deadline = t0 + TIME_BUDGET_MS;
    if (studs.length == 0 || studs[0].length == 0) {
      return new PackResult("anneal", new ArrayList<>(), 0, "ok");
    }

    PackResult seed = new GreedyPacker(catalog).pack(studs);
    List<PlacedPart> best = new ArrayList<>(seed.placed);
    List<PlacedPart> current = new ArrayList<>(seed.placed);
    int bestCount = best.size();
    int currentCount = current.size();
    int seedCount = seed.pieceCount();

    int h = studs.length;
    int w = studs[0].length;
    double temperature = Math.max(5.0, seedCount * 0.02);

    for (int iter = 0; iter < MAX_ITERS; iter++) {
      if (System.currentTimeMillis() > deadline) {
        break;
      }

      int wx = rng.nextInt(w);
      int wy = rng.nextInt(h);
      int ww = Math.min(WINDOW, w - wx);
      int wh = Math.min(WINDOW, h - wy);
      if (ww <= 0 || wh <= 0) {
        continue;
      }

      List<PlacedPart> candidate = rePackWindow(studs, current, wx, wy, ww, wh, w, h);
      int candCount = candidate.size();
      int delta = candCount - currentCount;

      boolean accept = false;
      if (delta <= 0) {
        accept = true;
      } else if (temperature > 1e-9) {
        accept = rng.nextDouble() < Math.exp(-delta / temperature);
      }

      if (accept) {
        current = candidate;
        currentCount = candCount;
        if (candCount < bestCount) {
          best = new ArrayList<>(candidate);
          bestCount = candCount;
        }
      }

      temperature *= 0.995;
    }

    long ms = System.currentTimeMillis() - t0;
    String status = bestCount < seedCount ? "improved" : "ok";
    return new PackResult("anneal", best, ms, status);
  }

  /**
   * Keep parts outside the window; clear the window and re-greedy with shuffled
   * footprint order.
   */
  private List<PlacedPart> rePackWindow(
      colorMatch.LegoElement[][] studs,
      List<PlacedPart> current,
      int wx,
      int wy,
      int ww,
      int wh,
      int gridW,
      int gridH) {
    List<PlacedPart> kept = new ArrayList<>();
    boolean[][] covered = new boolean[gridH][gridW];

    for (PlacedPart p : current) {
      if (partIntersectsWindow(p, wx, wy, ww, wh)) {
        continue;
      }
      kept.add(p);
      GreedyPacker.mark(covered, p.x, p.y, p.w, p.h);
    }

    // Mark null cells covered
    for (int y = 0; y < gridH; y++) {
      for (int x = 0; x < gridW; x++) {
        if (studs[y][x] == null) {
          covered[y][x] = true;
        }
      }
    }

    List<PlacedPart> result = new ArrayList<>(kept);
    for (int y = wy; y < wy + wh; y++) {
      for (int x = wx; x < wx + ww; x++) {
        if (covered[y][x]) {
          continue;
        }
        placeAtShuffled(studs, covered, result, x, y, gridW, gridH);
      }
    }

    // Safety: cover any holes left elsewhere (should be rare)
    for (int y = 0; y < gridH; y++) {
      for (int x = 0; x < gridW; x++) {
        if (!covered[y][x] && studs[y][x] != null) {
          placeAtShuffled(studs, covered, result, x, y, gridW, gridH);
        }
      }
    }
    return result;
  }

  private static boolean partIntersectsWindow(PlacedPart p, int wx, int wy, int ww, int wh) {
    return p.x < wx + ww && p.x + p.w > wx && p.y < wy + wh && p.y + p.h > wy;
  }

  private void placeAtShuffled(
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
    List<PlateCatalog.PlateSize> sizes =
        new ArrayList<>(catalog.footprintsForColor(colorId));
    Collections.shuffle(sizes, rng);

    for (PlateCatalog.PlateSize size : sizes) {
      if (GreedyPacker.fits(studs, covered, x, y, size.w, size.h, colorId, w, h)) {
        GreedyPacker.mark(covered, x, y, size.w, size.h);
        placed.add(new PlacedPart(
            size.partNum, colorId, el.colorName, x, y, size.w, size.h));
        return;
      }
    }
    covered[y][x] = true;
    placed.add(new PlacedPart("3024", colorId, el.colorName, x, y, 1, 1));
  }
}

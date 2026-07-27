import java.util.ArrayList;
import java.util.List;

/**
 * Row RLE + vertical merge packing (survey alg 2).
 * Strip-biased baseline for comparison against full 2D greedy.
 */
public class RlePacker {
  private final PlateCatalog catalog;

  public RlePacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(colorMatch.LegoElement[][] studs) {
    long t0 = System.currentTimeMillis();
    if (studs.length == 0 || studs[0].length == 0) {
      return new PackResult("rle", new ArrayList<>(), 0, "ok");
    }

    int h = studs.length;
    int w = studs[0].length;
    boolean[][] covered = new boolean[h][w];
    List<PlacedPart> placed = new ArrayList<>();

    // Phase A: horizontal RLE → 1-high strips
    for (int y = 0; y < h; y++) {
      int x = 0;
      while (x < w) {
        if (covered[y][x] || studs[y][x] == null) {
          if (studs[y][x] == null) {
            covered[y][x] = true;
          }
          x++;
          continue;
        }
        int colorId = studs[y][x].colorId;
        String colorName = studs[y][x].colorName;
        int start = x;
        while (x < w
            && !covered[y][x]
            && studs[y][x] != null
            && studs[y][x].colorId == colorId) {
          x++;
        }
        int runLen = x - start;
        emitStripRun(studs, covered, placed, start, y, runLen, colorId, colorName, w, h);
      }
    }

    // Phase B: stack matching strips into taller catalog plates
    placed = verticalMerge(placed);

    long ms = System.currentTimeMillis() - t0;
    return new PackResult("rle", placed, ms, "ok");
  }

  private void emitStripRun(
      colorMatch.LegoElement[][] studs,
      boolean[][] covered,
      List<PlacedPart> placed,
      int x0,
      int y,
      int runLen,
      int colorId,
      String colorName,
      int w,
      int h) {
    List<PlateCatalog.PlateSize> strips = oneHighForColor(colorId);
    int x = x0;
    int remaining = runLen;
    while (remaining > 0) {
      boolean placedOne = false;
      for (PlateCatalog.PlateSize size : strips) {
        if (size.w <= remaining
            && GreedyPacker.fits(studs, covered, x, y, size.w, size.h, colorId, w, h)) {
          GreedyPacker.mark(covered, x, y, size.w, size.h);
          placed.add(new PlacedPart(
              size.partNum, colorId, colorName, x, y, size.w, size.h));
          x += size.w;
          remaining -= size.w;
          placedOne = true;
          break;
        }
      }
      if (!placedOne) {
        GreedyPacker.mark(covered, x, y, 1, 1);
        placed.add(new PlacedPart("3024", colorId, colorName, x, y, 1, 1));
        x++;
        remaining--;
      }
    }
  }

  private List<PlateCatalog.PlateSize> oneHighForColor(int colorId) {
    List<PlateCatalog.PlateSize> out = new ArrayList<>();
    for (PlateCatalog.PlateSize p : catalog.footprintsForColor(colorId)) {
      if (p.h == 1) {
        out.add(p);
      }
    }
    return out;
  }

  /**
   * Merge vertically adjacent strips that share x-span and color when a taller
   * catalog footprint exists (e.g. two 1×4 → one 2×4).
   */
  private List<PlacedPart> verticalMerge(List<PlacedPart> initial) {
    List<PlacedPart> parts = new ArrayList<>(initial);
    boolean improved = true;
    while (improved) {
      improved = false;
      outer:
      for (int i = 0; i < parts.size(); i++) {
        PlacedPart a = parts.get(i);
        for (int j = 0; j < parts.size(); j++) {
          if (i == j) {
            continue;
          }
          PlacedPart b = parts.get(j);
          if (a.colorId != b.colorId || a.x != b.x || a.w != b.w) {
            continue;
          }
          // b sits directly below a
          if (b.y != a.y + a.h) {
            continue;
          }
          int newH = a.h + b.h;
          PlateCatalog.PlateSize taller = findFootprint(a.colorId, a.w, newH);
          if (taller == null) {
            continue;
          }
          List<PlacedPart> next = new ArrayList<>();
          for (int k = 0; k < parts.size(); k++) {
            if (k != i && k != j) {
              next.add(parts.get(k));
            }
          }
          next.add(new PlacedPart(
              taller.partNum, a.colorId, a.colorName, a.x, a.y, taller.w, taller.h));
          parts = next;
          improved = true;
          break outer;
        }
      }
    }
    return parts;
  }

  private PlateCatalog.PlateSize findFootprint(int colorId, int pw, int ph) {
    for (PlateCatalog.PlateSize s : catalog.footprintsForColor(colorId)) {
      if (s.w == pw && s.h == ph) {
        return s;
      }
    }
    return null;
  }
}

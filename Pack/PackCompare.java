import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Side-by-side comparison of packing modes. */
public class PackCompare {

  /** Two-mode compare (kept for callers); routes through {@link #compareAll}. */
  public static List<String> compare(PackResult a, PackResult b) {
    return compareAll(a, b);
  }

  /** Multi-mode table; deltas vs first result when it is present (typically greedy). */
  public static List<String> compareAll(PackResult... results) {
    return compareAll(Arrays.asList(results));
  }

  public static List<String> compareAll(List<PackResult> results) {
    List<String> lines = new ArrayList<>();
    if (results == null || results.isEmpty()) {
      lines.add("(no packing results)");
      return lines;
    }

    lines.add(String.format("%-12s %8s %10s  %s", "mode", "pieces", "time_ms", "status"));
    PackResult baseline = results.get(0);
    for (PackResult r : results) {
      lines.add(String.format(
          "%-12s %8d %10d  %s",
          r.modeName, r.pieceCount(), r.elapsedMs, r.status));
    }

    lines.add("Deltas vs " + baseline.modeName + " (pieces):");
    for (int i = 1; i < results.size(); i++) {
      PackResult r = results.get(i);
      int delta = r.pieceCount() - baseline.pieceCount();
      lines.add(String.format("  %-10s %+8d", r.modeName, delta));
    }

    // Top part|color diffs between baseline and each other mode (cap lines)
    lines.add("Top key differences vs " + baseline.modeName + ":");
    Map<String, Integer> baseCounts = counts(baseline);
    int shown = 0;
    for (int i = 1; i < results.size() && shown < 30; i++) {
      PackResult r = results.get(i);
      Map<String, Integer> other = counts(r);
      boolean any = false;
      for (String key : unionKeys(baseCounts, other)) {
        int da = baseCounts.getOrDefault(key, 0);
        int db = other.getOrDefault(key, 0);
        if (da != db) {
          if (!any) {
            lines.add("  [" + r.modeName + "]");
            any = true;
          }
          lines.add(String.format(
              "    %s  %s=%d %s=%d",
              key, baseline.modeName, da, r.modeName, db));
          if (++shown >= 30) {
            break;
          }
        }
      }
    }
    if (shown == 0) {
      lines.add("  (none)");
    }
    return lines;
  }

  private static Map<String, Integer> counts(PackResult r) {
    Map<String, Integer> m = new HashMap<>();
    for (PlacedPart p : r.placed) {
      String key = p.partNum + "|" + p.colorId + " (" + p.colorName + ")";
      m.put(key, m.getOrDefault(key, 0) + 1);
    }
    return m;
  }

  private static List<String> unionKeys(Map<String, Integer> a, Map<String, Integer> b) {
    List<String> keys = new ArrayList<>(a.keySet());
    for (String k : b.keySet()) {
      if (!a.containsKey(k)) {
        keys.add(k);
      }
    }
    keys.sort((x, y) -> {
      int dx = Math.abs(a.getOrDefault(x, 0) - b.getOrDefault(x, 0));
      int dy = Math.abs(a.getOrDefault(y, 0) - b.getOrDefault(y, 0));
      return Integer.compare(dy, dx);
    });
    return keys;
  }
}

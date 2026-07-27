import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** BOM formatting from placed parts (post-pack shopping list). */
public class PackBom {

  public static List<String> formatBom(PackResult result) {
    List<String> lines = new ArrayList<>();
    lines.add("Mode: " + result.modeName);
    lines.add("Status: " + result.status);
    lines.add("Time ms: " + result.elapsedMs);
    lines.add("Total pieces: " + result.pieceCount());

    // key: partNum|colorId -> count, keep a sample for name
    Map<String, Integer> counts = new HashMap<>();
    Map<String, PlacedPart> samples = new HashMap<>();
    for (PlacedPart p : result.placed) {
      String key = p.partNum + "|" + p.colorId;
      counts.put(key, counts.getOrDefault(key, 0) + 1);
      samples.putIfAbsent(key, p);
    }

    List<String> keys = new ArrayList<>(counts.keySet());
    keys.sort(Comparator
        .comparing((String k) -> counts.get(k)).reversed()
        .thenComparing(k -> samples.get(k).colorName)
        .thenComparing(k -> samples.get(k).partNum));

    lines.add("By part and color:");
    for (String key : keys) {
      PlacedPart s = samples.get(key);
      lines.add(String.format(
          "  %d  %s %dx%d  color %d %s",
          counts.get(key), s.partNum, s.w, s.h, s.colorId, s.colorName));
    }
    return lines;
  }
}

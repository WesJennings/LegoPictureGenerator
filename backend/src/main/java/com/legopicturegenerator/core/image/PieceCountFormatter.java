package com.legopicturegenerator.core.image;

import com.legopicturegenerator.core.color.ColorMatcher;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

/** Formats color tallies collected during matchImage into a shopping-list report. */
public class PieceCountFormatter {

  /**
   * Build report lines sorted by count descending, then color name.
   * Does not recount the stud grid — only formats the maps.
   */
  public static List<String> formatReport(
      Map<Integer, Integer> colorCounts,
      Map<Integer, ColorMatcher.LegoElement> colorSamples) {

    List<String> lines = new ArrayList<>();
    int total = 0;
    for (int n : colorCounts.values()) {
      total += n;
    }
    lines.add("Total pieces: " + total);

    List<Integer> colorIds = new ArrayList<>(colorCounts.keySet());
    colorIds.sort(Comparator
        .comparing((Integer id) -> colorCounts.get(id)).reversed()
        .thenComparing(id -> {
          ColorMatcher.LegoElement sample = colorSamples.get(id);
          return sample == null ? "" : sample.colorName;
        }));

    for (int id : colorIds) {
      int count = colorCounts.get(id);
      ColorMatcher.LegoElement sample = colorSamples.get(id);
      if (sample == null) {
        lines.add(String.format("  %d  color %d", count, id));
      } else {
        lines.add(String.format(
            "  %d  color %d %s (#%s)  part %s",
            count, id, sample.colorName, sample.rgbHex, sample.partNum));
      }
    }

    return lines;
  }
}

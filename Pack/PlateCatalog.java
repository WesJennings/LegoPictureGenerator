import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Plate sizes used by packers, plus DB lookup for which colors exist per part.
 */
public class PlateCatalog {
  public static final class PlateSize {
    public final String partNum;
    public final int w;
    public final int h;

    public PlateSize(String partNum, int w, int h) {
      this.partNum = partNum;
      this.w = w;
      this.h = h;
    }

    public int area() {
      return w * h;
    }
  }

  /** Canonical catalog (width × height in studs). Rotations added in footprints(). */
  private static final PlateSize[] BASE = {
      new PlateSize("41539", 8, 8),
      new PlateSize("3028", 6, 12),
      new PlateSize("3029", 4, 12),
      new PlateSize("3033", 6, 10),
      new PlateSize("3030", 4, 10),
      new PlateSize("3036", 6, 8),
      new PlateSize("3035", 4, 8),
      new PlateSize("2445", 2, 12),
      new PlateSize("3958", 6, 6),
      new PlateSize("3032", 4, 6),
      new PlateSize("60479", 1, 12),
      new PlateSize("3832", 2, 10),
      new PlateSize("3031", 4, 4),
      new PlateSize("4477", 1, 10),
      new PlateSize("3034", 2, 8),
      new PlateSize("3795", 2, 6),
      new PlateSize("3460", 1, 8),
      new PlateSize("3666", 1, 6),
      new PlateSize("3020", 2, 4),
      new PlateSize("3710", 1, 4),
      new PlateSize("3021", 2, 3),
      new PlateSize("3623", 1, 3),
      new PlateSize("3022", 2, 2),
      new PlateSize("3023", 1, 2),
      new PlateSize("3024", 1, 1),
  };

  private final Map<String, Set<Integer>> colorsByPart = new HashMap<>();
  private final List<PlateSize> footprints;

  public PlateCatalog(String dbPath) throws SQLException {
    loadAvailability(dbPath);
    footprints = buildFootprints();
  }

  /** All oriented footprints, largest area first. */
  public List<PlateSize> footprints() {
    return footprints;
  }

  public boolean available(String partNum, int colorId) {
    Set<Integer> colors = colorsByPart.get(partNum);
    return colors != null && colors.contains(colorId);
  }

  /** Footprints that exist for this color, largest first. */
  public List<PlateSize> footprintsForColor(int colorId) {
    List<PlateSize> out = new ArrayList<>();
    for (PlateSize p : footprints) {
      if (available(p.partNum, colorId)) {
        out.add(p);
      }
    }
    return out;
  }

  private void loadAvailability(String dbPath) throws SQLException {
    String sql = "SELECT DISTINCT part_num, color_id FROM elements WHERE part_num = ?";
    try (Connection conn = DriverManager.getConnection("jdbc:sqlite:" + dbPath);
         PreparedStatement ps = conn.prepareStatement(sql)) {
      Set<String> seen = new HashSet<>();
      for (PlateSize base : BASE) {
        if (!seen.add(base.partNum)) {
          continue;
        }
        ps.setString(1, base.partNum);
        Set<Integer> colors = new HashSet<>();
        try (ResultSet rs = ps.executeQuery()) {
          while (rs.next()) {
            colors.add(rs.getInt("color_id"));
          }
        }
        colorsByPart.put(base.partNum, colors);
      }
    }
  }

  private List<PlateSize> buildFootprints() {
    List<PlateSize> list = new ArrayList<>();
    Set<String> dedupe = new HashSet<>();
    for (PlateSize base : BASE) {
      addOriented(list, dedupe, base.partNum, base.w, base.h);
    }
    list.sort((a, b) -> {
      int c = Integer.compare(b.area(), a.area());
      if (c != 0) {
        return c;
      }
      return Integer.compare(Math.max(b.w, b.h), Math.max(a.w, a.h));
    });
    return Collections.unmodifiableList(list);
  }

  private static void addOriented(
      List<PlateSize> list, Set<String> dedupe, String partNum, int w, int h) {
    String k1 = partNum + ":" + w + "x" + h;
    if (dedupe.add(k1)) {
      list.add(new PlateSize(partNum, w, h));
    }
    if (w != h) {
      String k2 = partNum + ":" + h + "x" + w;
      if (dedupe.add(k2)) {
        list.add(new PlateSize(partNum, h, w));
      }
    }
  }
}

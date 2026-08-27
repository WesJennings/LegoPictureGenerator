package com.legopicturegenerator.core.color;

import com.legopicturegenerator.core.nativeengine.NativeEngine;
import java.awt.image.BufferedImage;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class ColorMatcher {
  public static final String DEFAULT_STUD_PART = "3024"; // Plate 1 x 1
  /** Load LEGO colors from the Rebrickable SQLite database. */
  public static List<LegoColor> loadColors(String dbPath) throws SQLException {
    List<LegoColor> colors = new ArrayList<>();
    String url = "jdbc:sqlite:" + dbPath;

    try (Connection conn = DriverManager.getConnection(url);
         Statement st = conn.createStatement();
         ResultSet rs = st.executeQuery(
             "SELECT id, name, rgb, is_trans FROM colors ORDER BY id")) {

      while (rs.next()) {
        int id = rs.getInt("id");
        String name = rs.getString("name");
        String rgb = rs.getString("rgb");
        boolean isTrans = isTransparent(rs.getString("is_trans"));
        colors.add(new LegoColor(id, name, rgb, isTrans));
      }
    }

    return colors;
  }

  /**
   * Load opaque elements that exist for a part (part_num + color_id pairs).
   * Skips transparent colors and the unknown color id (-1).
   */
  public static List<LegoElement> loadElements(String dbPath, String partNum)
      throws SQLException {
    List<LegoElement> elements = new ArrayList<>();
    String url = "jdbc:sqlite:" + dbPath;
    String sql =
        "SELECT DISTINCT e.part_num, c.id AS color_id, c.name AS color_name, c.rgb, c.is_trans "
            + "FROM elements e "
            + "JOIN colors c ON c.id = e.color_id "
            + "WHERE e.part_num = ? "
            + "AND c.id >= 0 "
            + "ORDER BY c.id";

    try (Connection conn = DriverManager.getConnection(url);
         PreparedStatement ps = conn.prepareStatement(sql)) {
      ps.setString(1, partNum);
      try (ResultSet rs = ps.executeQuery()) {
        while (rs.next()) {
          if (isTransparent(rs.getString("is_trans"))) {
            continue;
          }
          elements.add(new LegoElement(
              rs.getString("part_num"),
              rs.getInt("color_id"),
              rs.getString("color_name"),
              rs.getString("rgb")));
        }
      }
    }

    if (elements.isEmpty()) {
      throw new SQLException("No opaque elements found for part " + partNum);
    }

    return elements;
  }

  /** Rebrickable dumps use t/f or True/False for is_trans. */
  private static boolean isTransparent(String isTrans) {
    if (isTrans == null) {
      return false;
    }
    String v = isTrans.trim();
    return v.equalsIgnoreCase("t") || v.equalsIgnoreCase("true");
  }

  /** Nearest element by RGB Euclidean distance (C++ engine). */
  public static LegoElement nearest(int argb, List<LegoElement> palette) {
    NativeEngine.ensureLoaded();
    int[] palR = new int[palette.size()];
    int[] palG = new int[palette.size()];
    int[] palB = new int[palette.size()];
    for (int i = 0; i < palette.size(); i++) {
      palR[i] = palette.get(i).r;
      palG[i] = palette.get(i).g;
      palB[i] = palette.get(i).b;
    }
    int[] matched = new int[1];
    int[] idx = NativeEngine.matchIndices(new int[] {argb}, 1, 1, palR, palG, palB, matched);
    return palette.get(idx[0]);
  }

  /**
   * Replace each stud pixel with the nearest LEGO element color.
   * If elementsOut is non-null, fills it with the matched element per stud.
   */
  public static BufferedImage matchImage(
      BufferedImage studs,
      List<LegoElement> palette,
      LegoElement[][] elementsOut) {
    return matchImage(studs, palette, elementsOut, null, null);
  }

  /**
   * Same as {@link #matchImage(BufferedImage, List, LegoElement[][])}, and while
   * matching optionally tallies color counts / one sample element per colorId.
   *
   * @param colorCountsOut  may be null; otherwise colorId -> count
   * @param colorSamplesOut may be null; otherwise colorId -> first matched element
   */
  public static BufferedImage matchImage(
      BufferedImage studs,
      List<LegoElement> palette,
      LegoElement[][] elementsOut,
      Map<Integer, Integer> colorCountsOut,
      Map<Integer, LegoElement> colorSamplesOut) {

    NativeEngine.ensureLoaded();
    int width = studs.getWidth();
    int height = studs.getHeight();
    int[] grid = studs.getRGB(0, 0, width, height, null, 0, width);
    int[] palR = new int[palette.size()];
    int[] palG = new int[palette.size()];
    int[] palB = new int[palette.size()];
    for (int i = 0; i < palette.size(); i++) {
      palR[i] = palette.get(i).r;
      palG[i] = palette.get(i).g;
      palB[i] = palette.get(i).b;
    }
    int[] matched = new int[width * height];
    int[] indices = NativeEngine.matchIndices(
        grid, width, height, palR, palG, palB, matched);

    BufferedImage output =
        new BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB);
    output.setRGB(0, 0, width, height, matched, 0, width);

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        LegoElement el = palette.get(indices[y * width + x]);
        if (elementsOut != null) {
          elementsOut[y][x] = el;
        }
        if (colorCountsOut != null) {
          colorCountsOut.merge(el.colorId, 1, Integer::sum);
        }
        if (colorSamplesOut != null) {
          colorSamplesOut.putIfAbsent(el.colorId, el);
        }
      }
    }

    return output;
  }

  /** One official LEGO color from the catalog. */
  public static class LegoColor {
    public final int id;
    public final String name;
    public final String rgbHex;
    public final boolean isTrans;
    public final int r;
    public final int g;
    public final int b;

    public LegoColor(int id, String name, String rgbHex, boolean isTrans) {
      this.id = id;
      this.name = name;
      this.rgbHex = rgbHex;
      this.isTrans = isTrans;
      this.r = Integer.parseInt(rgbHex.substring(0, 2), 16);
      this.g = Integer.parseInt(rgbHex.substring(2, 4), 16);
      this.b = Integer.parseInt(rgbHex.substring(4, 6), 16);
    }

    public int toArgb() {
      return (0xFF << 24) | (r << 16) | (g << 8) | b;
    }

    @Override
    public String toString() {
      return id + "\t" + name + "\t#" + rgbHex
          + (isTrans ? " (trans)" : "");
    }
  }

  /** A concrete LEGO element: part + color. */
  public static class LegoElement {
    public final String partNum;
    public final int colorId;
    public final String colorName;
    public final String rgbHex;
    public final int r;
    public final int g;
    public final int b;

    public LegoElement(String partNum, int colorId, String colorName, String rgbHex) {
      this.partNum = partNum;
      this.colorId = colorId;
      this.colorName = colorName;
      this.rgbHex = rgbHex;
      this.r = Integer.parseInt(rgbHex.substring(0, 2), 16);
      this.g = Integer.parseInt(rgbHex.substring(2, 4), 16);
      this.b = Integer.parseInt(rgbHex.substring(4, 6), 16);
    }

    public int toArgb() {
      return (0xFF << 24) | (r << 16) | (g << 8) | b;
    }

    @Override
    public String toString() {
      return partNum + " / " + colorId + " " + colorName + " #" + rgbHex;
    }
  }
}

package com.legopicturegenerator.core.nativeengine;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.image.ImageSampler;
import com.legopicturegenerator.core.image.LegoRenderer;
import com.legopicturegenerator.core.pack.AnnealPacker;
import com.legopicturegenerator.core.pack.ComponentGreedyPacker;
import com.legopicturegenerator.core.pack.DlxPacker;
import com.legopicturegenerator.core.pack.ExactIlpPacker;
import com.legopicturegenerator.core.pack.GreedyPacker;
import com.legopicturegenerator.core.pack.PackResult;
import com.legopicturegenerator.core.pack.PlacedPart;
import com.legopicturegenerator.core.pack.PlateCatalog;
import com.legopicturegenerator.core.pack.RlePacker;
import java.awt.image.BufferedImage;
import java.nio.file.Path;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.Statement;
import java.util.List;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

/**
 * Functional parity for the C++ engine: sampling, matching, packing coverage,
 * and render dimensions. Algorithm logic is the Java original translated 1:1.
 */
class NativeParityTest {

  @TempDir
  Path tmp;

  private PlateCatalog catalog;
  private List<ColorMatcher.LegoElement> palette;

  @BeforeEach
  void setup() throws Exception {
    NativeEngine.ensureLoaded();
    Path db = tmp.resolve("bricks.db");
    try (Connection conn = DriverManager.getConnection("jdbc:sqlite:" + db);
         Statement st = conn.createStatement()) {
      st.executeUpdate(
          "CREATE TABLE colors (id INTEGER, name TEXT, rgb TEXT, is_trans TEXT)");
      st.executeUpdate(
          "CREATE TABLE elements (part_num TEXT, color_id INTEGER)");
      st.executeUpdate("INSERT INTO colors VALUES (0, 'Black', '05131D', 'f')");
      st.executeUpdate("INSERT INTO colors VALUES (15, 'White', 'FFFFFF', 'f')");
      for (int color : new int[] {0, 15}) {
        st.executeUpdate("INSERT INTO elements VALUES ('3024', " + color + ")");
        st.executeUpdate("INSERT INTO elements VALUES ('3023', " + color + ")");
        st.executeUpdate("INSERT INTO elements VALUES ('3022', " + color + ")");
      }
    }
    catalog = new PlateCatalog(db.toString());
    palette = ColorMatcher.loadElements(db.toString(), ColorMatcher.DEFAULT_STUD_PART);
  }

  @Test
  void samplerAveragesAndPreservesGeometry() {
    BufferedImage src = new BufferedImage(20, 10, BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < 10; y++) {
      for (int x = 0; x < 20; x++) {
        src.setRGB(x, y, x < 10 ? 0xFF000000 : 0xFFFFFFFF);
      }
    }
    BufferedImage out = ImageSampler.toStudGrid(src, 2);
    assertEquals(2, out.getWidth());
    assertEquals(1, out.getHeight());
    assertEquals(0xFF000000, out.getRGB(0, 0));
    assertEquals(0xFFFFFFFF, out.getRGB(1, 0));
  }

  @Test
  void nearestColorPicksBlackOrWhite() {
    ColorMatcher.LegoElement black = ColorMatcher.nearest(0xFF000000, palette);
    ColorMatcher.LegoElement white = ColorMatcher.nearest(0xFFFFFFFF, palette);
    assertEquals(0, black.colorId);
    assertEquals(15, white.colorId);
  }

  @Test
  void allPackersCoverEveryStudOnCheckerSplit() {
    ColorMatcher.LegoElement[][] studs = solidSplit(8, 8);
    PackResult greedy = new GreedyPacker(catalog).pack(studs);
    PackResult rle = new RlePacker(catalog).pack(studs);
    PackResult component = new ComponentGreedyPacker(catalog).pack(studs);
    PackResult ilp = new ExactIlpPacker(catalog).pack(studs);
    PackResult dlx = new DlxPacker(catalog).pack(studs);
    PackResult anneal = new AnnealPacker(catalog).pack(studs);

    for (PackResult r : List.of(greedy, rle, component, ilp, dlx, anneal)) {
      assertEquals(64, coveredStuds(r), r.modeName + " must cover 64 studs");
      assertTrue(r.pieceCount() > 0);
      assertTrue(r.pieceCount() <= 64);
    }

    // 8×8 split with 2×2 available: each half is 4×8 → eight 2×2 per color = 16
    assertEquals(16, greedy.pieceCount());
    assertEquals(16, component.pieceCount());
    assertEquals(16, ilp.pieceCount());
    assertEquals(16, dlx.pieceCount());
    assertTrue(anneal.pieceCount() <= greedy.pieceCount());
    assertEquals("optimal", ilp.status);
    assertEquals("optimal", dlx.status);
  }

  @Test
  void renderProducesExpectedPixelSize() {
    ColorMatcher.LegoElement[][] studs = solidSplit(4, 3);
    BufferedImage matched = new BufferedImage(4, 3, BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < 3; y++) {
      for (int x = 0; x < 4; x++) {
        matched.setRGB(x, y, studs[y][x].toArgb());
      }
    }
    BufferedImage studsImg = LegoRenderer.renderStuds(matched, 8);
    assertEquals(32, studsImg.getWidth());
    assertEquals(24, studsImg.getHeight());

    PackResult greedy = new GreedyPacker(catalog).pack(studs);
    BufferedImage packed = LegoRenderer.renderPacked(greedy, studs, 8);
    assertEquals(32, packed.getWidth());
    assertEquals(24, packed.getHeight());
    // Backdrop or plate plastic should not be fully transparent
    assertTrue((packed.getRGB(0, 0) >>> 24) == 0xFF);
  }

  private ColorMatcher.LegoElement[][] solidSplit(int w, int h) {
    ColorMatcher.LegoElement black = palette.stream()
        .filter(e -> e.colorId == 0).findFirst().orElseThrow();
    ColorMatcher.LegoElement white = palette.stream()
        .filter(e -> e.colorId == 15).findFirst().orElseThrow();
    ColorMatcher.LegoElement[][] studs = new ColorMatcher.LegoElement[h][w];
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        studs[y][x] = x < w / 2 ? black : white;
      }
    }
    return studs;
  }

  private static int coveredStuds(PackResult result) {
    int n = 0;
    for (PlacedPart p : result.placed) {
      n += p.w * p.h;
    }
    return n;
  }
}

package com.legopicturegenerator.application;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.legopicturegenerator.domain.JobConfig;
import com.legopicturegenerator.domain.PackMode;
import com.legopicturegenerator.domain.PipelineResult;
import com.legopicturegenerator.infrastructure.CatalogProvider;
import java.awt.image.BufferedImage;
import java.nio.file.Files;
import java.nio.file.Path;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.Statement;
import java.util.EnumSet;
import javax.imageio.ImageIO;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class PipelineServiceTest {

  @TempDir
  Path tmp;

  /** Minimal bricks.db: black + white 1x1/1x2 plates. */
  private Path fixtureDb() throws Exception {
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
      }
    }
    return db;
  }

  private Path checkerboardPng(int size) throws Exception {
    BufferedImage img = new BufferedImage(size, size, BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        boolean darkHalf = x < size / 2;
        img.setRGB(x, y, darkHalf ? 0xFF000000 : 0xFFFFFFFF);
      }
    }
    Path file = tmp.resolve("input.png");
    ImageIO.write(img, "png", file.toFile());
    return file;
  }

  @Test
  void endToEndGreedyRun() throws Exception {
    CatalogProvider catalogs = new CatalogProvider(fixtureDb());
    PipelineService pipeline = new PipelineService(catalogs);

    Path out = tmp.resolve("job-out");
    // 160×160 source with blockSize 8 → 20×20 stud grid
    JobConfig cfg = new JobConfig(
        checkerboardPng(160), out, 8, 8, EnumSet.of(PackMode.GREEDY));

    PipelineResult result = pipeline.run(cfg, PipelineService.ProgressListener.NOOP);

    assertEquals(20, result.gridWidth());
    assertEquals(20, result.gridHeight());
    assertEquals(400, result.totalStuds());

    // Structured BOM total equals the packer's piece count
    PipelineResult.ModeResult greedy = result.modeResults().get(0);
    int bomTotal = greedy.bom().stream()
        .mapToInt(PipelineResult.BomRow::count).sum();
    assertEquals(greedy.pieceCount(), bomTotal);

    // Half black + half white grid must cover all studs
    int studsCovered = greedy.bom().stream()
        .mapToInt(r -> r.count() * r.w() * r.h()).sum();
    assertEquals(400, studsCovered);

    for (String artifact : new String[] {
        "matched.png", "lego-studs.png", "lego-greedy.png",
        "bom-greedy.txt", "placements-greedy.json", "color-counts.txt"}) {
      assertTrue(Files.isRegularFile(out.resolve(artifact)), artifact + " missing");
    }
    assertTrue(result.studBom().isEmpty());
  }

  @Test
  void classicStudWidthYieldsSensibleGridOnSmallImage() throws Exception {
    CatalogProvider catalogs = new CatalogProvider(fixtureDb());
    PipelineService pipeline = new PipelineService(catalogs);
    Path out = tmp.resolve("job-classic");
    // 320×480 with old B=80 → 4×6; classic width 54 → 54×81
    Path input = tmp.resolve("classic-input.png");
    java.awt.image.BufferedImage img =
        new java.awt.image.BufferedImage(320, 480, java.awt.image.BufferedImage.TYPE_INT_ARGB);
    for (int y = 0; y < 480; y++) {
      for (int x = 0; x < 320; x++) {
        img.setRGB(x, y, x < 160 ? 0xFF000000 : 0xFFFFFFFF);
      }
    }
    ImageIO.write(img, "png", input.toFile());
    JobConfig cfg = new JobConfig(
        input,
        out,
        JobConfig.DEFAULT_BLOCK_SIZE,
        JobConfig.CLASSIC_STUD_WIDTH,
        8,
        EnumSet.of(PackMode.GREEDY),
        false);

    PipelineResult result = pipeline.run(cfg, PipelineService.ProgressListener.NOOP);
    assertEquals(54, result.gridWidth());
    assertEquals(81, result.gridHeight());
    assertTrue(result.totalStuds() > 100);
  }

  @Test
  void includeStudBomWritesOneByOneList() throws Exception {
    CatalogProvider catalogs = new CatalogProvider(fixtureDb());
    PipelineService pipeline = new PipelineService(catalogs);
    Path out = tmp.resolve("job-studs");
    JobConfig cfg = new JobConfig(
        checkerboardPng(160),
        out,
        8,
        0,
        8,
        EnumSet.of(PackMode.GREEDY),
        true);

    PipelineResult result = pipeline.run(cfg, PipelineService.ProgressListener.NOOP);

    assertTrue(Files.isRegularFile(out.resolve("bom-studs.txt")));
    assertEquals(2, result.studBom().size());
    int studTotal = result.studBom().stream().mapToInt(PipelineResult.BomRow::count).sum();
    assertEquals(400, studTotal);
    for (PipelineResult.BomRow row : result.studBom()) {
      assertEquals(1, row.w());
      assertEquals(1, row.h());
    }
  }
}

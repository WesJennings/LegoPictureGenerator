package com.legopicturegenerator.application;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.image.ImageSampler;
import com.legopicturegenerator.core.image.LegoRenderer;
import com.legopicturegenerator.core.image.PieceCountFormatter;
import com.legopicturegenerator.core.pack.AnnealPacker;
import com.legopicturegenerator.core.pack.ComponentGreedyPacker;
import com.legopicturegenerator.core.pack.DlxPacker;
import com.legopicturegenerator.core.pack.ExactIlpPacker;
import com.legopicturegenerator.core.pack.GreedyPacker;
import com.legopicturegenerator.core.pack.PackBom;
import com.legopicturegenerator.core.pack.PackResult;
import com.legopicturegenerator.core.pack.PlacedPart;
import com.legopicturegenerator.core.pack.RlePacker;
import com.legopicturegenerator.domain.JobConfig;
import com.legopicturegenerator.domain.JobStatus;
import com.legopicturegenerator.domain.PackMode;
import com.legopicturegenerator.domain.PipelineResult;
import com.legopicturegenerator.infrastructure.CatalogProvider;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import javax.imageio.ImageIO;

/**
 * Runs the full mosaic pipeline for one {@link JobConfig}:
 * decode, sample to studs, color match, pack, render, write artifacts.
 * No HTTP or job-lifecycle knowledge lives here.
 */
public final class PipelineService {

  /** Stage callback so the job layer can persist progress. */
  public interface ProgressListener {
    void onStage(JobStatus stage);

    ProgressListener NOOP = stage -> {};
  }

  private final CatalogProvider catalogs;
  private final ObjectMapper json = new ObjectMapper();

  public PipelineService(CatalogProvider catalogs) {
    this.catalogs = catalogs;
  }

  /**
   * Sample + match + pack only (no artifact writes). Used by piece-target search.
   */
  public int probePieceCount(BufferedImage src, int blockSize, PackMode mode)
      throws InterruptedException {
    BufferedImage grid = ImageSampler.toStudGridByBlockSize(src, blockSize);
    checkInterrupted();
    int gh = grid.getHeight();
    int gw = grid.getWidth();
    ColorMatcher.LegoElement[][] studs = new ColorMatcher.LegoElement[gh][gw];
    ColorMatcher.matchImage(grid, catalogs.palette(), studs, null, null);
    checkInterrupted();
    PackResult result = runPacker(mode, studs);
    checkInterrupted();
    return result.pieceCount();
  }

  public PipelineResult run(JobConfig cfg, ProgressListener progress)
      throws IOException, InterruptedException {
    Files.createDirectories(cfg.outputDirectory());
    Map<String, String> artifacts = new LinkedHashMap<>();

    progress.onStage(JobStatus.PREPROCESSING);
    BufferedImage src = ImageIO.read(cfg.inputPath().toFile());
    if (src == null) {
      throw new IOException("Could not decode input image");
    }
    return runWithSource(src, cfg, progress, artifacts);
  }

  private PipelineResult runWithSource(
      BufferedImage src,
      JobConfig cfg,
      ProgressListener progress,
      Map<String, String> artifacts)
      throws IOException, InterruptedException {
    BufferedImage grid = cfg.usesStudWidthSampling()
        ? ImageSampler.toStudGrid(src, cfg.targetStudWidth())
        : ImageSampler.toStudGridByBlockSize(src, cfg.blockSize());
    checkInterrupted();

    progress.onStage(JobStatus.MATCHING);
    int gh = grid.getHeight();
    int gw = grid.getWidth();
    ColorMatcher.LegoElement[][] studs = new ColorMatcher.LegoElement[gh][gw];
    Map<Integer, Integer> colorCounts = new HashMap<>();
    Map<Integer, ColorMatcher.LegoElement> colorSamples = new HashMap<>();
    BufferedImage matched =
        ColorMatcher.matchImage(grid, catalogs.palette(), studs, colorCounts, colorSamples);

    writePng(matched, cfg.outputDirectory().resolve("matched.png"));
    artifacts.put("matched", "matched.png");

    List<String> countReport = PieceCountFormatter.formatReport(colorCounts, colorSamples);
    Files.write(cfg.outputDirectory().resolve("color-counts.txt"), countReport);
    artifacts.put("colorCounts", "color-counts.txt");

    List<PipelineResult.BomRow> studBom = List.of();
    if (cfg.includeStudBom()) {
      studBom = studBomRows(colorCounts, colorSamples);
      Files.write(cfg.outputDirectory().resolve("bom-studs.txt"), PackBom.formatStudBom(studBom));
      artifacts.put("bomStuds", "bom-studs.txt");
    }
    checkInterrupted();

    progress.onStage(JobStatus.PACKING);
    List<PipelineResult.ModeResult> modeResults = new ArrayList<>();
    Map<PackMode, PackResult> packResults = new LinkedHashMap<>();
    for (PackMode mode : PackMode.values()) {
      if (!cfg.modes().contains(mode)) {
        continue;
      }
      PackResult result = runPacker(mode, studs);
      packResults.put(mode, result);
      checkInterrupted();
    }

    progress.onStage(JobStatus.RENDERING);
    BufferedImage legoStuds = LegoRenderer.renderStuds(matched, cfg.renderStudSizePx());
    writePng(legoStuds, cfg.outputDirectory().resolve("lego-studs.png"));
    artifacts.put("legoStuds", "lego-studs.png");

    for (Map.Entry<PackMode, PackResult> e : packResults.entrySet()) {
      String mode = e.getKey().modeName;
      PackResult result = e.getValue();

      BufferedImage packed =
          LegoRenderer.renderPacked(result, studs, cfg.renderStudSizePx());
      String renderName = "lego-" + mode + ".png";
      writePng(packed, cfg.outputDirectory().resolve(renderName));
      artifacts.put("lego" + capitalize(mode), renderName);

      String bomName = "bom-" + mode + ".txt";
      Files.write(cfg.outputDirectory().resolve(bomName), PackBom.formatBom(result));
      artifacts.put("bom" + capitalize(mode), bomName);

      String placementsName = "placements-" + mode + ".json";
      json.writerWithDefaultPrettyPrinter()
          .writeValue(cfg.outputDirectory().resolve(placementsName).toFile(), result.placed);
      artifacts.put("placements" + capitalize(mode), placementsName);

      modeResults.add(new PipelineResult.ModeResult(
          mode, result.status, result.elapsedMs, result.pieceCount(), bomRows(result)));
      checkInterrupted();
    }

    return new PipelineResult(
        gw,
        gh,
        gw * gh,
        colorCountRows(colorCounts, colorSamples),
        studBom,
        modeResults,
        artifacts);
  }

  /** One BOM line per color: count of 1×1 plates (part from the matched element). */
  private static List<PipelineResult.BomRow> studBomRows(
      Map<Integer, Integer> colorCounts,
      Map<Integer, ColorMatcher.LegoElement> colorSamples) {
    List<PipelineResult.BomRow> rows = new ArrayList<>();
    for (Map.Entry<Integer, Integer> e : colorCounts.entrySet()) {
      ColorMatcher.LegoElement sample = colorSamples.get(e.getKey());
      String partNum = sample != null ? sample.partNum : ColorMatcher.DEFAULT_STUD_PART;
      String colorName = sample != null ? sample.colorName : "";
      rows.add(new PipelineResult.BomRow(
          partNum, 1, 1, e.getKey(), colorName, e.getValue()));
    }
    rows.sort(Comparator
        .comparing(PipelineResult.BomRow::count).reversed()
        .thenComparing(PipelineResult.BomRow::colorName)
        .thenComparing(PipelineResult.BomRow::partNum));
    return rows;
  }

  private PackResult runPacker(PackMode mode, ColorMatcher.LegoElement[][] studs) {
    return switch (mode) {
      case GREEDY -> new GreedyPacker(catalogs.plateCatalog()).pack(studs);
      case ILP -> new ExactIlpPacker(catalogs.plateCatalog()).pack(studs);
      case RLE -> new RlePacker(catalogs.plateCatalog()).pack(studs);
      case COMPONENT -> new ComponentGreedyPacker(catalogs.plateCatalog()).pack(studs);
      case DLX -> new DlxPacker(catalogs.plateCatalog()).pack(studs);
      case ANNEAL -> new AnnealPacker(catalogs.plateCatalog()).pack(studs);
    };
  }

  private static List<PipelineResult.BomRow> bomRows(PackResult result) {
    Map<String, Integer> counts = new HashMap<>();
    Map<String, PlacedPart> samples = new HashMap<>();
    for (PlacedPart p : result.placed) {
      String key = p.partNum + "|" + p.colorId;
      counts.merge(key, 1, Integer::sum);
      samples.putIfAbsent(key, p);
    }
    List<PipelineResult.BomRow> rows = new ArrayList<>();
    for (Map.Entry<String, Integer> e : counts.entrySet()) {
      PlacedPart s = samples.get(e.getKey());
      rows.add(new PipelineResult.BomRow(
          s.partNum, s.w, s.h, s.colorId, s.colorName, e.getValue()));
    }
    rows.sort(Comparator
        .comparing(PipelineResult.BomRow::count).reversed()
        .thenComparing(PipelineResult.BomRow::colorName)
        .thenComparing(PipelineResult.BomRow::partNum));
    return rows;
  }

  private static List<PipelineResult.ColorCountRow> colorCountRows(
      Map<Integer, Integer> colorCounts,
      Map<Integer, ColorMatcher.LegoElement> colorSamples) {
    List<PipelineResult.ColorCountRow> rows = new ArrayList<>();
    for (Map.Entry<Integer, Integer> e : colorCounts.entrySet()) {
      ColorMatcher.LegoElement sample = colorSamples.get(e.getKey());
      rows.add(new PipelineResult.ColorCountRow(
          e.getKey(),
          sample != null ? sample.colorName : "",
          sample != null ? sample.rgbHex : "",
          e.getValue()));
    }
    rows.sort(Comparator.comparing(PipelineResult.ColorCountRow::count).reversed()
        .thenComparing(PipelineResult.ColorCountRow::colorName));
    return rows;
  }

  private static void writePng(BufferedImage image, Path path) throws IOException {
    if (!ImageIO.write(image, "png", path.toFile())) {
      throw new IOException("No PNG writer available for " + path);
    }
  }

  private static void checkInterrupted() throws InterruptedException {
    if (Thread.interrupted()) {
      throw new InterruptedException("pipeline interrupted");
    }
  }

  private static String capitalize(String s) {
    return Character.toUpperCase(s.charAt(0)) + s.substring(1);
  }
}

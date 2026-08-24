package com.legopicturegenerator.cli;

import com.legopicturegenerator.application.PipelineService;
import com.legopicturegenerator.domain.JobConfig;
import com.legopicturegenerator.domain.PackMode;
import com.legopicturegenerator.domain.PipelineResult;
import com.legopicturegenerator.infrastructure.CatalogProvider;
import java.nio.file.Path;
import java.util.EnumSet;
import java.util.Set;

/**
 * Offline entrypoint — same pipeline as the website, no copied algorithm code.
 *
 * <p>Usage: CliApplication [input] [outputDir] [blockSize] [modes]
 * e.g. {@code samples/Jarvis.png runtime/cli 80 greedy,ilp,dlx}
 */
public final class CliApplication {

  public static void main(String[] args) throws Exception {
    Path input = Path.of(args.length > 0 ? args[0] : "samples/Jarvis.png");
    Path outputDir = Path.of(args.length > 1 ? args[1] : "runtime/cli");
    int blockSize = args.length > 2
        ? Integer.parseInt(args[2]) : JobConfig.DEFAULT_BLOCK_SIZE;
    Set<PackMode> modes = args.length > 3 ? parseModes(args[3]) : EnumSet.of(PackMode.GREEDY);

    CatalogProvider catalogs = new CatalogProvider(
        Path.of(System.getenv().getOrDefault("LEGO_DB_PATH", "data/bricks.db")));
    PipelineService pipeline = new PipelineService(catalogs);

    JobConfig cfg = new JobConfig(
        input, outputDir, blockSize, JobConfig.DEFAULT_RENDER_STUD_PX, modes);
    PipelineResult result = pipeline.run(
        cfg, stage -> System.out.println("[stage] " + stage));

    System.out.printf("Grid: %d x %d (%d studs)%n",
        result.gridWidth(), result.gridHeight(), result.totalStuds());
    for (PipelineResult.ModeResult mode : result.modeResults()) {
      System.out.printf("%-10s pieces=%-6d time=%-7dms status=%s%n",
          mode.mode(), mode.pieceCount(), mode.elapsedMs(), mode.status());
    }
    System.out.println("Artifacts written to " + outputDir);
  }

  private static Set<PackMode> parseModes(String raw) {
    Set<PackMode> modes = EnumSet.noneOf(PackMode.class);
    for (String part : raw.split(",")) {
      modes.add(PackMode.fromModeName(part.trim()));
    }
    return modes;
  }
}

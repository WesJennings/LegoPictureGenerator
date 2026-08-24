package com.legopicturegenerator.infrastructure;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.legopicturegenerator.domain.JobConfig;
import com.legopicturegenerator.domain.JobManifest;
import java.nio.file.Path;
import java.util.List;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class FileJobRepositorySizingTest {

  @TempDir
  Path tmp;

  @Test
  void piecesSizingPersistsTargetsAndForcesAutoSizingFalse() throws Exception {
    FileJobRepository repo = new FileJobRepository(tmp);
    JobManifest m = repo.createJob(
        JobConfig.DEFAULT_BLOCK_SIZE,
        JobConfig.SIZING_PIECES,
        0,
        800,
        false,
        JobConfig.DEFAULT_RENDER_STUD_PX,
        List.of("dlx"));

    assertEquals(JobConfig.SIZING_PIECES, m.sizingMode);
    assertEquals(800, m.targetPieceCount);
    assertFalse(m.autoSizing);
    assertTrue(m.isPieceSizing());

    JobManifest loaded = repo.findJob(m.id).orElseThrow();
    assertEquals(JobConfig.SIZING_PIECES, loaded.effectiveSizingMode());
    assertEquals(800, loaded.targetPieceCount);
    assertEquals(List.of("dlx"), loaded.modes);
  }
}

package com.legopicturegenerator.infrastructure;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.pack.PlateCatalog;
import java.nio.file.Files;
import java.nio.file.Path;
import java.sql.SQLException;
import java.util.List;

/**
 * Loads the LEGO palette and plate catalog from bricks.db once per process.
 * Both are immutable afterwards and shared by every job.
 */
public final class CatalogProvider {
  private final List<ColorMatcher.LegoElement> palette;
  private final PlateCatalog plateCatalog;
  private final String dbPath;

  public CatalogProvider(Path dbPath) throws SQLException {
    if (!Files.isRegularFile(dbPath)) {
      throw new IllegalStateException("bricks.db not found at " + dbPath
          + " — see data/README.md for how to obtain it");
    }
    this.dbPath = dbPath.toString();
    this.palette = ColorMatcher.loadElements(this.dbPath, ColorMatcher.DEFAULT_STUD_PART);
    this.plateCatalog = new PlateCatalog(this.dbPath);
  }

  public List<ColorMatcher.LegoElement> palette() {
    return palette;
  }

  public PlateCatalog plateCatalog() {
    return plateCatalog;
  }

  public String dbPath() {
    return dbPath;
  }
}

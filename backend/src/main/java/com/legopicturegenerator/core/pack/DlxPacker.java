package com.legopicturegenerator.core.pack;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.nativeengine.NativeEngine;

/**
 * Min-cardinality exact cover via Algorithm X–style search (fewest-options).
 * Algorithm body is the C++ engine; logic is unchanged from the Java original.
 */
public class DlxPacker {
  private final PlateCatalog catalog;

  public DlxPacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(ColorMatcher.LegoElement[][] studs) {
    return NativeEngine.pack("dlx", studs, catalog);
  }
}

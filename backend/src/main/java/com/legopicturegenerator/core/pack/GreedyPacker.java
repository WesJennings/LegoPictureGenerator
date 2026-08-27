package com.legopicturegenerator.core.pack;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.nativeengine.NativeEngine;

/**
 * Greedy largest-first packing with multi-order repair.
 * Algorithm body is the C++ engine; logic is unchanged from the Java original.
 */
public class GreedyPacker {
  private final PlateCatalog catalog;

  public GreedyPacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(ColorMatcher.LegoElement[][] studs) {
    return NativeEngine.pack("greedy", studs, catalog);
  }
}

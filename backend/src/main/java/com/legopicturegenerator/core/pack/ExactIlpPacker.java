package com.legopicturegenerator.core.pack;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.nativeengine.NativeEngine;

/**
 * Mode 2 — catalog exact cover via branch-and-bound (min placements).
 * Algorithm body is the C++ engine; logic is unchanged from the Java original
 * (64-cell bitmask limit, greedy fallback, same time budgets).
 */
public class ExactIlpPacker {
  private final PlateCatalog catalog;

  public ExactIlpPacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(ColorMatcher.LegoElement[][] studs) {
    return NativeEngine.pack("ilp", studs, catalog);
  }
}

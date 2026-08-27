package com.legopicturegenerator.core.pack;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.nativeengine.NativeEngine;

/**
 * Connected-component greedy packing (survey alg 3).
 * Algorithm body is the C++ engine; logic is unchanged from the Java original.
 */
public class ComponentGreedyPacker {
  private final PlateCatalog catalog;

  public ComponentGreedyPacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(ColorMatcher.LegoElement[][] studs) {
    return NativeEngine.pack("component", studs, catalog);
  }
}

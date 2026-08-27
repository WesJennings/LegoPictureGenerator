package com.legopicturegenerator.core.pack;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.nativeengine.NativeEngine;

/**
 * Simulated annealing local search. Seeds from greedy, then re-packs windows
 * with shuffled size order. C++ uses {@code java.util.Random(42)}'s LCG so
 * shuffle/accept sequences match the original Java implementation.
 */
public class AnnealPacker {
  private final PlateCatalog catalog;

  public AnnealPacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(ColorMatcher.LegoElement[][] studs) {
    return NativeEngine.pack("anneal", studs, catalog);
  }
}

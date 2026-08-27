package com.legopicturegenerator.core.pack;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.nativeengine.NativeEngine;

/**
 * Row RLE + vertical merge packing (survey alg 2).
 * Algorithm body is the C++ engine; logic is unchanged from the Java original.
 */
public class RlePacker {
  private final PlateCatalog catalog;

  public RlePacker(PlateCatalog catalog) {
    this.catalog = catalog;
  }

  public PackResult pack(ColorMatcher.LegoElement[][] studs) {
    return NativeEngine.pack("rle", studs, catalog);
  }
}

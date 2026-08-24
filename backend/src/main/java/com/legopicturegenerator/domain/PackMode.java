package com.legopicturegenerator.domain;

/** The six packing algorithms, in walkthrough numbering order. */
public enum PackMode {
  GREEDY("greedy"),
  ILP("ilp"),
  RLE("rle"),
  COMPONENT("component"),
  DLX("dlx"),
  ANNEAL("anneal");

  public final String modeName;

  PackMode(String modeName) {
    this.modeName = modeName;
  }

  public static PackMode fromModeName(String name) {
    for (PackMode m : values()) {
      if (m.modeName.equalsIgnoreCase(name)) {
        return m;
      }
    }
    throw new IllegalArgumentException("Unknown pack mode: " + name);
  }
}

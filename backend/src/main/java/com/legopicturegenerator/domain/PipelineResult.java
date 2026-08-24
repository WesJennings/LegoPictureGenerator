package com.legopicturegenerator.domain;

import java.util.List;
import java.util.Map;

/** Structured output of one pipeline run; the UI never parses text files. */
public record PipelineResult(
    int gridWidth,
    int gridHeight,
    int totalStuds,
    List<ColorCountRow> colorCounts,
    /** 1×1 stud shopping list (empty unless the job requested it). */
    List<BomRow> studBom,
    List<ModeResult> modeResults,
    Map<String, String> artifacts) {

  /** One matched color and how many 1x1 studs use it. */
  public record ColorCountRow(int colorId, String colorName, String rgbHex, int count) {}

  /** One BOM line: how many of one (part, color) to buy. */
  public record BomRow(
      String partNum, int w, int h, int colorId, String colorName, int count) {}

  /** Outcome of one packing mode. */
  public record ModeResult(
      String mode,
      String status,
      long elapsedMs,
      int pieceCount,
      List<BomRow> bom) {}
}

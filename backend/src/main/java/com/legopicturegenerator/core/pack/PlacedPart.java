package com.legopicturegenerator.core.pack;

/** One placed LEGO plate on the stud grid (origin top-left, size in studs). */
public class PlacedPart {
  public final String partNum;
  public final int colorId;
  public final String colorName;
  public final int x;
  public final int y;
  public final int w;
  public final int h;

  public PlacedPart(
      String partNum, int colorId, String colorName, int x, int y, int w, int h) {
    this.partNum = partNum;
    this.colorId = colorId;
    this.colorName = colorName;
    this.x = x;
    this.y = y;
    this.w = w;
    this.h = h;
  }

  public int area() {
    return w * h;
  }

  @Override
  public String toString() {
    return partNum + " " + w + "x" + h + " color " + colorId + " " + colorName
        + " @ (" + x + "," + y + ")";
  }
}

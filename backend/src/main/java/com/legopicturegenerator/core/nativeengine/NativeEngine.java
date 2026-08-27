package com.legopicturegenerator.core.nativeengine;

import com.legopicturegenerator.core.color.ColorMatcher;
import com.legopicturegenerator.core.pack.PackResult;
import com.legopicturegenerator.core.pack.PlacedPart;
import com.legopicturegenerator.core.pack.PlateCatalog;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * JNI entry to the C++ engine ({@code liblegocore}). Java keeps types, JDBC
 * catalog loading, and the HTTP/job host; sampling, matching, packing, and
 * rendering run natively with the same algorithm logic.
 */
public final class NativeEngine {
  private static final Object LOCK = new Object();
  private static boolean loaded;

  private NativeEngine() {}

  public static void ensureLoaded() {
    synchronized (LOCK) {
      if (loaded) {
        return;
      }
      loadLibrary();
      loaded = true;
    }
  }

  private static void loadLibrary() {
    String explicit = System.getProperty("lego.native.lib", System.getenv("LEGO_NATIVE_LIB"));
    if (explicit != null && !explicit.isBlank()) {
      System.load(Path.of(explicit).toAbsolutePath().toString());
      return;
    }

    Path[] candidates = new Path[] {
        Path.of("native/build/liblegocore.so"),
        Path.of("../native/build/liblegocore.so"),
        Path.of("backend/src/main/resources/native/liblegocore.so"),
        Path.of("src/main/resources/native/liblegocore.so"),
    };
    for (Path p : candidates) {
      if (Files.isRegularFile(p)) {
        System.load(p.toAbsolutePath().toString());
        return;
      }
    }

    try (InputStream in = NativeEngine.class.getResourceAsStream("/native/liblegocore.so")) {
      if (in == null) {
        throw new UnsatisfiedLinkError(
            "liblegocore.so not found. Build it with native/build.sh (or make setup/test).");
      }
      Path tmp = Files.createTempFile("liblegocore", ".so");
      tmp.toFile().deleteOnExit();
      Files.copy(in, tmp, StandardCopyOption.REPLACE_EXISTING);
      System.load(tmp.toAbsolutePath().toString());
    } catch (IOException e) {
      throw new UnsatisfiedLinkError("Could not extract liblegocore.so: " + e.getMessage());
    }
  }

  public static native int studCountFor(int srcWidth, int srcHeight, int blockSize);

  public static native int blockSizeForTargetStuds(int srcWidth, int srcHeight, int targetStuds);

  public static native int[] sampleByBlockSize(
      int[] src, int sw, int sh, int blockSize, int[] outWH);

  public static native int[] sampleByWidth(
      int[] src, int sw, int sh, int targetW, int[] outWH);

  public static native int[] matchIndices(
      int[] grid, int width, int height, int[] palR, int[] palG, int[] palB, int[] outMatched);

  public static native PackResult pack(
      String mode,
      int w,
      int h,
      int[] colorIds,
      String[] colorNames,
      boolean[] present,
      int[] colorKeys,
      String[] partNums,
      int[] widths,
      int[] heights,
      int[] counts);

  public static native int[] renderStuds(int[] grid, int cols, int rows, int studPx);

  public static native int[] renderPacked(
      int[] grid, int cols, int rows, int studPx, int[] xs, int[] ys, int[] ws, int[] hs);

  public static PackResult pack(
      String mode, ColorMatcher.LegoElement[][] studs, PlateCatalog catalog) {
    ensureLoaded();
    if (studs.length == 0 || studs[0].length == 0) {
      return new PackResult(mode, List.of(), 0, "ok");
    }
    int h = studs.length;
    int w = studs[0].length;
    int n = w * h;
    int[] colorIds = new int[n];
    String[] colorNames = new String[n];
    boolean[] present = new boolean[n];
    Map<Integer, List<PlateCatalog.PlateSize>> byColor = new LinkedHashMap<>();
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        int i = y * w + x;
        ColorMatcher.LegoElement el = studs[y][x];
        if (el == null) {
          colorNames[i] = "";
          continue;
        }
        present[i] = true;
        colorIds[i] = el.colorId;
        colorNames[i] = el.colorName;
        byColor.computeIfAbsent(el.colorId, catalog::footprintsForColor);
      }
    }

    int[] colorKeys = new int[byColor.size()];
    int[] counts = new int[byColor.size()];
    List<String> partNums = new ArrayList<>();
    List<Integer> widths = new ArrayList<>();
    List<Integer> heights = new ArrayList<>();
    int ci = 0;
    for (Map.Entry<Integer, List<PlateCatalog.PlateSize>> e : byColor.entrySet()) {
      colorKeys[ci] = e.getKey();
      counts[ci] = e.getValue().size();
      for (PlateCatalog.PlateSize s : e.getValue()) {
        partNums.add(s.partNum);
        widths.add(s.w);
        heights.add(s.h);
      }
      ci++;
    }
    return pack(
        mode,
        w,
        h,
        colorIds,
        colorNames,
        present,
        colorKeys,
        partNums.toArray(String[]::new),
        toIntArray(widths),
        toIntArray(heights),
        counts);
  }

  private static int[] toIntArray(List<Integer> list) {
    int[] a = new int[list.size()];
    for (int i = 0; i < list.size(); i++) {
      a[i] = list.get(i);
    }
    return a;
  }

  public static int[] gridArgb(ColorMatcher.LegoElement[][] studs) {
    int h = studs.length;
    int w = studs[0].length;
    int[] argb = new int[w * h];
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        ColorMatcher.LegoElement el = studs[y][x];
        argb[y * w + x] = el == null ? 0 : el.toArgb();
      }
    }
    return argb;
  }

  public static int[][] unpackCoords(List<PlacedPart> placed) {
    int n = placed.size();
    int[][] out = new int[4][n];
    for (int i = 0; i < n; i++) {
      PlacedPart p = placed.get(i);
      out[0][i] = p.x;
      out[1][i] = p.y;
      out[2][i] = p.w;
      out[3][i] = p.h;
    }
    return out;
  }
}

package com.legopicturegenerator.api;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Iterator;
import javax.imageio.ImageIO;
import javax.imageio.ImageReader;
import javax.imageio.stream.ImageInputStream;

/**
 * Validates a stored upload before any pixel decoding happens:
 * magic-byte type sniffing, then header-only dimension limits.
 */
public final class UploadValidator {
  public static final long MAX_UPLOAD_BYTES = 25L * 1024 * 1024;
  public static final long MAX_PIXELS = 25_000_000L;

  private UploadValidator() {}

  public static final class InvalidUploadException extends RuntimeException {
    public InvalidUploadException(String message) {
      super(message);
    }
  }

  /** Throws {@link InvalidUploadException} unless the file is a sane PNG/JPEG. */
  public static void validate(Path file) throws IOException {
    if (Files.size(file) > MAX_UPLOAD_BYTES) {
      throw new InvalidUploadException("Upload exceeds 25 MB limit");
    }
    if (!isPngOrJpeg(file)) {
      throw new InvalidUploadException("Only PNG and JPEG images are supported");
    }
    checkDimensionsFromHeader(file);
  }

  /** Magic bytes only — extension and client MIME type are untrusted. */
  private static boolean isPngOrJpeg(Path file) throws IOException {
    byte[] head = new byte[4];
    try (InputStream in = Files.newInputStream(file)) {
      int read = in.readNBytes(head, 0, 4);
      if (read < 4) {
        return false;
      }
    }
    boolean png = (head[0] & 0xFF) == 0x89 && head[1] == 0x50
        && head[2] == 0x4E && head[3] == 0x47;
    boolean jpeg = (head[0] & 0xFF) == 0xFF && (head[1] & 0xFF) == 0xD8
        && (head[2] & 0xFF) == 0xFF;
    return png || jpeg;
  }

  /**
   * Reads declared dimensions from the image header without decoding pixels,
   * so a decompression-bomb file is rejected before it can allocate memory.
   */
  private static void checkDimensionsFromHeader(Path file) throws IOException {
    try (ImageInputStream iis = ImageIO.createImageInputStream(file.toFile())) {
      Iterator<ImageReader> readers = ImageIO.getImageReaders(iis);
      if (!readers.hasNext()) {
        throw new InvalidUploadException("Unreadable image file");
      }
      ImageReader reader = readers.next();
      try {
        reader.setInput(iis);
        long w = reader.getWidth(0);
        long h = reader.getHeight(0);
        if (w < 1 || h < 1) {
          throw new InvalidUploadException("Image has invalid dimensions");
        }
        if (w * h > MAX_PIXELS) {
          throw new InvalidUploadException(
              "Image is too large (max " + (MAX_PIXELS / 1_000_000) + " megapixels)");
        }
      } finally {
        reader.dispose();
      }
    }
  }
}

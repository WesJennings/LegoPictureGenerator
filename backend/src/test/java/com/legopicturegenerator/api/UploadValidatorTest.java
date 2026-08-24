package com.legopicturegenerator.api;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import javax.imageio.ImageIO;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class UploadValidatorTest {

  @TempDir
  Path tmp;

  private Path writePng(int w, int h) throws IOException {
    Path file = tmp.resolve("img.png");
    ImageIO.write(new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB),
        "png", file.toFile());
    return file;
  }

  @Test
  void acceptsSmallPng() throws IOException {
    assertDoesNotThrow(() -> UploadValidator.validate(writePng(50, 50)));
  }

  @Test
  void rejectsRenamedTextFile() throws IOException {
    Path fake = tmp.resolve("fake.png");
    Files.writeString(fake, "definitely not an image");
    assertThrows(UploadValidator.InvalidUploadException.class,
        () -> UploadValidator.validate(fake));
  }

  @Test
  void rejectsOversizedDeclaredDimensions() throws IOException {
    // 6000x6000 = 36 MP > 25 MP cap; rejected from the header before decode
    Path big = writePng(6000, 6000);
    assertThrows(UploadValidator.InvalidUploadException.class,
        () -> UploadValidator.validate(big));
  }

  @Test
  void rejectsTruncatedFile() throws IOException {
    Path stub = tmp.resolve("stub.png");
    Files.write(stub, new byte[] {(byte) 0x89});
    assertThrows(UploadValidator.InvalidUploadException.class,
        () -> UploadValidator.validate(stub));
  }
}

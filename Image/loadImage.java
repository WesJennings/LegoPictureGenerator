import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.sql.SQLException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import javax.imageio.ImageIO;
import java.awt.Graphics2D;
import java.awt.RenderingHints;

public class loadImage {
  public static final int BLOCK_SIZE = 80;
  private static final String INPUT_IMAGE = "Image/resources/Jarvis.png";
  private static final String ARTIFACTS_DIR = "artifacts";
  private static final String OUTPUT_IMAGE = ARTIFACTS_DIR + "/output.png";
  private static final String OUTPUT_LEGO_IMAGE = ARTIFACTS_DIR + "/output_lego.png";
  private static final String OUTPUT_LEGO_GREEDY = ARTIFACTS_DIR + "/output_lego_greedy.png";
  private static final String OUTPUT_LEGO_ILP = ARTIFACTS_DIR + "/output_lego_ilp.png";
  private static final String OUTPUT_LEGO_RLE = ARTIFACTS_DIR + "/output_lego_rle.png";
  private static final String OUTPUT_LEGO_COMPONENT = ARTIFACTS_DIR + "/output_lego_component.png";
  private static final String OUTPUT_LEGO_DLX = ARTIFACTS_DIR + "/output_lego_dlx.png";
  private static final String OUTPUT_LEGO_ANNEAL = ARTIFACTS_DIR + "/output_lego_anneal.png";
  private static final String COLOR_COUNTS_FILE = ARTIFACTS_DIR + "/color_counts.txt";
  private static final String BOM_GREEDY_FILE = ARTIFACTS_DIR + "/bom_greedy.txt";
  private static final String BOM_ILP_FILE = ARTIFACTS_DIR + "/bom_ilp.txt";
  private static final String BOM_RLE_FILE = ARTIFACTS_DIR + "/bom_rle.txt";
  private static final String BOM_COMPONENT_FILE = ARTIFACTS_DIR + "/bom_component.txt";
  private static final String BOM_DLX_FILE = ARTIFACTS_DIR + "/bom_dlx.txt";
  private static final String BOM_ANNEAL_FILE = ARTIFACTS_DIR + "/bom_anneal.txt";
  private static final String BOM_COMPARE_FILE = ARTIFACTS_DIR + "/bom_compare.txt";
  private static final String DB_PATH = "data/bricks.db";

  public static void main(String[] args) throws IOException, SQLException {
    Files.createDirectories(Path.of(ARTIFACTS_DIR));

    //Read in image from resources
    BufferedImage image = ImageIO.read(new File(INPUT_IMAGE));
    int width = image.getWidth();
    int height = image.getHeight();
    //Pixel color is image.getRGB(x,y)

    image = MergePixels(image, width, height, BLOCK_SIZE);

    image = ReScale(image, width, height, BLOCK_SIZE);

    List<colorMatch.LegoElement> palette =
        colorMatch.loadElements(DB_PATH, colorMatch.DEFAULT_STUD_PART);
    colorMatch.LegoElement[][] studs =
        new colorMatch.LegoElement[image.getHeight()][image.getWidth()];
    Map<Integer, Integer> colorCounts = new HashMap<>();
    Map<Integer, colorMatch.LegoElement> colorSamples = new HashMap<>();
    image = colorMatch.matchImage(image, palette, studs, colorCounts, colorSamples);

    List<String> report = pieceCount.formatReport(colorCounts, colorSamples);
    for (String line : report) {
      System.out.println(line);
    }
    Files.write(Path.of(COLOR_COUNTS_FILE), report);

    // Optional packing compare: comment out this block to skip.
    PlateCatalog catalog = new PlateCatalog(DB_PATH);
    PackResult greedy = new GreedyPacker(catalog).pack(studs);
    PackResult ilp = new ExactIlpPacker(catalog).pack(studs);
    PackResult rle = new RlePacker(catalog).pack(studs);
    PackResult component = new ComponentGreedyPacker(catalog).pack(studs);
    PackResult dlx = new DlxPacker(catalog).pack(studs);
    PackResult anneal = new AnnealPacker(catalog).pack(studs);

    List<PackResult> all = Arrays.asList(greedy, ilp, rle, component, dlx, anneal);
    List<String> compare = PackCompare.compareAll(all);
    for (String line : compare) {
      System.out.println(line);
    }
    Files.write(Path.of(BOM_GREEDY_FILE), PackBom.formatBom(greedy));
    Files.write(Path.of(BOM_ILP_FILE), PackBom.formatBom(ilp));
    Files.write(Path.of(BOM_RLE_FILE), PackBom.formatBom(rle));
    Files.write(Path.of(BOM_COMPONENT_FILE), PackBom.formatBom(component));
    Files.write(Path.of(BOM_DLX_FILE), PackBom.formatBom(dlx));
    Files.write(Path.of(BOM_ANNEAL_FILE), PackBom.formatBom(anneal));
    Files.write(Path.of(BOM_COMPARE_FILE), compare);

    ImageIO.write(image, "png", new File(OUTPUT_IMAGE));

    // Optional LEGO-look renders: comment out any you want to skip.
    int studPx = legoRender.DEFAULT_STUD_SIZE_PX;
    BufferedImage legoLook = legoRender.renderStuds(image, studPx);
    ImageIO.write(legoLook, "png", new File(OUTPUT_LEGO_IMAGE));
    ImageIO.write(
        legoRender.renderPacked(greedy, studs, studPx), "png", new File(OUTPUT_LEGO_GREEDY));
    ImageIO.write(
        legoRender.renderPacked(ilp, studs, studPx), "png", new File(OUTPUT_LEGO_ILP));
    ImageIO.write(
        legoRender.renderPacked(rle, studs, studPx), "png", new File(OUTPUT_LEGO_RLE));
    ImageIO.write(
        legoRender.renderPacked(component, studs, studPx),
        "png",
        new File(OUTPUT_LEGO_COMPONENT));
    ImageIO.write(
        legoRender.renderPacked(dlx, studs, studPx), "png", new File(OUTPUT_LEGO_DLX));
    ImageIO.write(
        legoRender.renderPacked(anneal, studs, studPx), "png", new File(OUTPUT_LEGO_ANNEAL));
  }

  private static BufferedImage MergePixels(BufferedImage image, int width, int height, int blockSize){
    //Load rgb into array
    //ARGB pixels, stored row by row, left to right, top to bottom
    int[] src = image.getRGB(0,0,width,height,null,0,width);
  
    //Destination array for adjusted pixels
    int[] dst = new int[src.length];

    for(int by = 0; by + blockSize <= height; by += blockSize){
      for(int bx = 0; bx + blockSize <= width; bx += blockSize){
        long sumR = 0, sumG = 0, sumB = 0;
        int count = blockSize * blockSize;

        //Find average for each block
        for(int y = by; y < by + blockSize; y++){
          int row = y * width;
          for(int x = bx; x < bx + blockSize; x++){
            int pixel = src[row + x];
            sumR += (pixel >> 16) & 0xFF;
            sumG += (pixel >> 8) & 0xFF;
            sumB += pixel & 0xFF;
          }
        }

        int avgR = (int) (sumR / count);
        int avgG = (int) (sumG / count);
        int avgB = (int) (sumB / count);
        //Rebuild pixel
        int avg = (0xFF << 24) | (avgR << 16) | (avgG << 8) | avgB;

        //Write average to each pixel in block
        for(int y = by; y < by + blockSize; y++){
          int row = y * width;
          for(int x = bx; x < bx + blockSize; x++){
            dst[row + x] = avg;
          }
        }
      }
    }

    //Write back to image
    image.setRGB(0,0,width,height,dst,0,width);
    return image;
  }

  private static BufferedImage ReScale(BufferedImage image, int width, int height, int blockSize) {
    int newWidth = width / blockSize;
    int newHeight = height / blockSize;
    //New blank image
    BufferedImage scaled = new BufferedImage(newWidth, newHeight, image.getType());

    //Draw into the blank image
    Graphics2D g = scaled.createGraphics();
    //When shrinking, prioritize averaging over sharp edges
    g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
    //Take current image and draw it into the blank image with the new width and height
    g.drawImage(image, 0, 0, newWidth, newHeight, null);
    g.dispose();
    return scaled;
  }
}

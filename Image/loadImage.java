import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import javax.imageio.ImageIO;

public class loadImage {
  public static final int BLOCK_SIZE = 16;

  public static void main(String[] args) throws IOException {
    //Read in image from resources  
    BufferedImage image = ImageIO.read(new File("resources/BackgroundSS.png"));
    int width = image.getWidth();
    int height = image.getHeight();
    //Pixel color is image.getRGB(x,y)

    image = MergePixels(image, width, height, BLOCK_SIZE);

    ImageIO.write(image,"png", new File("resources/output.png"));
  }

  private static BufferedImage MergePixels(BufferedImage image, int width, int height, int blockSize){
    //Load rgb into array
    //ARGB pixels, stored row by row, left to right, top to bottom
    int[] src = image.getRGB(0,0,width,height,null,0,width);
  
    //Destination array for adjusted pixels
    int[] dst = new int[src.length];

    //Loop blocks 
    for(int by = 0; by < height; by += blockSize){

      //Compute end coordinates so it doesnt overflow
      int yEnd = Math.min(by + blockSize, height);

      for(int bx = 0; bx < width; bx += blockSize){
        //Compute end coord
        int xEnd = Math.min(bx + blockSize, width);

        // Select top left color to color whole block 
        // FUTURE: change this to be a function that gets the average color of the block
        int rgb = AverageColors(src, by, bx, yEnd, xEnd, width);

        //Loop through the individual block
        for(int y = by; y < yEnd; y++){
          int row = y * width;

          for (int x = bx; x < xEnd; x++){
            //Assing block color to this pixel
            dst[row + x] = rgb;
          }
        }
      }
    }
    //Create the new output image 
    BufferedImage output = new BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB);
    
    //set new RGBs to output
    output.setRGB(0,0,width, height, dst, 0, width);

    return output;
  }  

  private static int AverageColors(int[] src, int by, int bx, int yEnd, int xEnd, int width){
    //Bit masking
    int A = 0;
    int R = 0;
    int G = 0;
    int B = 0;
    int actualWidth = xEnd - bx;
    int actualHeight = yEnd - by;
    int count = actualWidth * actualHeight;

    for(int y = by; y < yEnd; y++){
        int row = y * width;
        for(int x = bx; x < xEnd; x++){
          int pixel = src[row + x];
          
          A += (pixel >>> 24) & 0xFF;
          R += (pixel >>> 16) & 0xFF;
          G += (pixel >>> 8) & 0xFF;
          B += (pixel) & 0xFF;
        }
    }

    A /= count;
    R /= count;
    G /= count;
    B /= count;

    return (A << 24) | (R << 16) | (G << 8) | B;
  }
}

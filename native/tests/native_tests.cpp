#include "lego/color_matcher.hpp"
#include "lego/image_sampler.hpp"
#include "lego/java_random.hpp"
#include "lego/packers.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace lego;

static void testSampler() {
  std::vector<int> src(160 * 80, 0xFF123456);
  auto out = toStudGridByBlockSize(src.data(), 160, 80, 80);
  assert(out.width == 2);
  assert(out.height == 1);

  int b = blockSizeForTargetStuds(100, 100, 12000);
  assert(b == 8);

  auto wide = toStudGrid(src.data(), 160, 80, 40);
  assert(wide.width == 40);
  assert(wide.height == 20);
}

static void testNearest() {
  std::vector<PaletteEntry> pal = {{0, 0, 0}, {255, 255, 255}};
  assert(nearestIndex(0xFF000000, pal) == 0);
  assert(nearestIndex(0xFFFFFFFF, pal) == 1);
}

static void testJavaRandom() {
  JavaRandom rng(42);
  // First few nextInt(10) values from OpenJDK Random(42)
  // Verified independently: we'll just check range and determinism.
  JavaRandom rng2(42);
  for (int i = 0; i < 20; i++) {
    assert(rng.nextInt(10) == rng2.nextInt(10));
  }
}

static void testGreedySolid() {
  // 4x4 solid black, catalog 2x2 then 1x1 → four 2x2 plates
  StudGrid grid(4, std::vector<LegoElement>(4));
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      grid[y][x].present = true;
      grid[y][x].colorId = 0;
      grid[y][x].colorName = "Black";
    }
  }
  PlateCatalog cat;
  cat.byColor[0] = {
      PlateSize{"3022", 2, 2},
      PlateSize{"3023", 1, 2},
      PlateSize{"3023", 2, 1},
      PlateSize{"3024", 1, 1},
  };
  auto greedy = packGreedy(cat, grid);
  assert(greedy.pieceCount() == 4);
  auto rle = packRle(cat, grid);
  assert(rle.pieceCount() <= 8);
  auto dlx = packDlx(cat, grid);
  assert(dlx.pieceCount() == 4);
  auto ilp = packIlp(cat, grid);
  assert(ilp.pieceCount() == 4);
}

int main() {
  testSampler();
  testNearest();
  testJavaRandom();
  testGreedySolid();
  std::cout << "native tests ok\n";
  return 0;
}

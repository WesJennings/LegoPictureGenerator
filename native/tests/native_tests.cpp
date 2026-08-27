#include "lego/color_matcher.hpp"
#include "lego/image_sampler.hpp"
#include "lego/java_random.hpp"
#include "lego/packers.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
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

  int autoB = blockSizeForTargetStuds(4000, 3000, 4000);
  int studs = studCountFor(4000, 3000, autoB);
  assert(autoB >= MIN_BLOCK_SIZE && autoB <= MAX_BLOCK_SIZE);
  assert(std::abs(studs - 4000) < 1000);

  std::vector<int> tiny(10 * 10, 0xFFFF0000);
  auto clamp = toStudGrid(tiny.data(), 10, 10, 54);
  assert(clamp.width == 10);
  assert(clamp.height == 10);

  std::vector<int> strip(1000 * 3, 0xFFABCDEF);
  auto h1 = toStudGrid(strip.data(), 1000, 3, 54);
  assert(h1.width == 54);
  assert(h1.height >= 1);

  std::vector<int> gray(7 * 5, 0xFF808080);
  auto avg = toStudGrid(gray.data(), 7, 5, 3);
  assert(avg.width == 3);
  for (int p : avg.argb) {
    assert(p == 0xFF808080);
  }

  std::vector<int> split(20 * 10);
  for (int y = 0; y < 10; y++) {
    for (int x = 0; x < 20; x++) {
      split[y * 20 + x] = x < 10 ? 0xFF000000 : 0xFFFFFFFF;
    }
  }
  auto two = toStudGrid(split.data(), 20, 10, 2);
  assert(two.argb[0] == 0xFF000000);
  assert(two.argb[1] == 0xFFFFFFFF);
}

static void testNearest() {
  std::vector<PaletteEntry> pal = {{0, 0, 0}, {255, 255, 255}};
  assert(nearestIndex(0xFF000000, pal) == 0);
  assert(nearestIndex(0xFFFFFFFF, pal) == 1);
}

static void testJavaRandom() {
  JavaRandom rng(42);
  JavaRandom rng2(42);
  for (int i = 0; i < 20; i++) {
    assert(rng.nextInt(10) == rng2.nextInt(10));
  }
}

static void testGreedySolid() {
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

static LegoElement el(int colorId, const std::string& name, int r, int g, int b) {
  LegoElement e;
  e.present = true;
  e.colorId = colorId;
  e.colorName = name;
  e.r = r;
  e.g = g;
  e.b = b;
  e.rgbHex = colorId == 0 ? "05131D" : "FFFFFF";
  e.partNum = "3024";
  return e;
}

static int coveredStuds(const PackResult& result) {
  int n = 0;
  for (const auto& p : result.placed) {
    n += p.w * p.h;
  }
  return n;
}

static void testPackersCoverSplit() {
  LegoElement black = el(0, "Black", 5, 19, 29);
  LegoElement white = el(15, "White", 255, 255, 255);
  StudGrid grid(8, std::vector<LegoElement>(8));
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      grid[y][x] = x < 4 ? black : white;
    }
  }
  PlateCatalog cat;
  std::vector<PlateSize> fp = {
      PlateSize{"3022", 2, 2},
      PlateSize{"3023", 1, 2},
      PlateSize{"3023", 2, 1},
      PlateSize{"3024", 1, 1},
  };
  cat.byColor[0] = fp;
  cat.byColor[15] = fp;

  auto greedy = packGreedy(cat, grid);
  auto rle = packRle(cat, grid);
  auto component = packComponent(cat, grid);
  auto ilp = packIlp(cat, grid);
  auto dlx = packDlx(cat, grid);
  auto anneal = packAnneal(cat, grid);
  for (const auto& r : {greedy, rle, component, ilp, dlx, anneal}) {
    assert(coveredStuds(r) == 64);
    assert(r.pieceCount() > 0);
    assert(r.pieceCount() <= 64);
  }
  assert(greedy.pieceCount() == 16);
  assert(component.pieceCount() == 16);
  assert(ilp.pieceCount() == 16);
  assert(dlx.pieceCount() == 16);
  assert(anneal.pieceCount() <= greedy.pieceCount());
  assert(ilp.status == "optimal");
  assert(dlx.status == "optimal");
}

int main() {
  testSampler();
  testNearest();
  testJavaRandom();
  testGreedySolid();
  testPackersCoverSplit();
  std::cout << "native tests ok\n";
  return 0;
}

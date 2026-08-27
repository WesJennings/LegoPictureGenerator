#include "lego/color_matcher.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace lego {

int nearestIndex(int argb, const std::vector<PaletteEntry>& palette) {
  uint32_t p = static_cast<uint32_t>(argb);
  int r = (p >> 16) & 0xFF;
  int g = (p >> 8) & 0xFF;
  int b = p & 0xFF;

  int best = 0;
  int64_t bestDist = std::numeric_limits<int64_t>::max();
  for (size_t i = 0; i < palette.size(); i++) {
    int64_t dr = r - palette[i].r;
    int64_t dg = g - palette[i].g;
    int64_t db = b - palette[i].b;
    int64_t dist = dr * dr + dg * dg + db * db;
    if (dist < bestDist) {
      bestDist = dist;
      best = static_cast<int>(i);
    }
  }
  return best;
}

MatchResult matchImage(const int* studArgb, int width, int height,
                       const std::vector<PaletteEntry>& palette) {
  MatchResult out;
  int n = width * height;
  out.matchedArgb.resize(n);
  out.paletteIndex.resize(n);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int i = y * width + x;
      int idx = nearestIndex(studArgb[i], palette);
      const PaletteEntry& el = palette[idx];
      out.paletteIndex[i] = idx;
      out.matchedArgb[i] = (0xFF << 24) | (el.r << 16) | (el.g << 8) | el.b;
    }
  }
  return out;
}

}  // namespace lego

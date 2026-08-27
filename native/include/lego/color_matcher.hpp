#pragma once

#include "lego/types.hpp"

#include <vector>

namespace lego {

struct PaletteEntry {
  int r = 0;
  int g = 0;
  int b = 0;
};

/** Index of nearest palette color by Euclidean RGB (same as Java ColorMatcher). */
int nearestIndex(int argb, const std::vector<PaletteEntry>& palette);

struct MatchResult {
  std::vector<int> matchedArgb;
  std::vector<int> paletteIndex;
};

MatchResult matchImage(const int* studArgb, int width, int height,
                       const std::vector<PaletteEntry>& palette);

}  // namespace lego

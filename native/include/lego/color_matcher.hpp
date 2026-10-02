#pragma once

#include "lego/types.hpp"

#include <vector>

namespace lego {

/** Perceptually uniform Lab-like coordinates (Ottosson OKLab). */
struct OkLab {
  float L = 0;
  float a = 0;
  float b = 0;
};

struct PaletteEntry {
  int r = 0;
  int g = 0;
  int b = 0;
  OkLab ok;
};

/** sRGB 0–255 → OKLab. */
OkLab srgbToOklab(int r, int g, int b);

/** Palette slot with sRGB and precomputed OKLab. */
PaletteEntry makePaletteEntry(int r, int g, int b);

/** Index of nearest palette color by Euclidean OKLab. Ties keep the first minimum. */
int nearestIndex(int argb, const std::vector<PaletteEntry>& palette);

struct MatchResult {
  std::vector<int> matchedArgb;
  std::vector<int> paletteIndex;
};

MatchResult matchImage(const int* studArgb, int width, int height,
                       const std::vector<PaletteEntry>& palette);

}  // namespace lego

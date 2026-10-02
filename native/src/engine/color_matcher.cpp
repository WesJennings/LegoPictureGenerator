#include "lego/color_matcher.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace lego {
namespace {

float srgbChannelToLinear(int c) {
  float s = static_cast<float>(c) / 255.0f;
  if (s <= 0.04045f) {
    return s / 12.92f;
  }
  return std::pow((s + 0.055f) / 1.055f, 2.4f);
}

}  // namespace

OkLab srgbToOklab(int r, int g, int b) {
  float lr = srgbChannelToLinear(r);
  float lg = srgbChannelToLinear(g);
  float lb = srgbChannelToLinear(b);

  float l = std::cbrt(0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb);
  float m = std::cbrt(0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb);
  float s = std::cbrt(0.0883024619f * lr + 0.2817188376f * lg + 0.6309787005f * lb);

  OkLab out;
  out.L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
  out.a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
  out.b = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
  return out;
}

PaletteEntry makePaletteEntry(int r, int g, int b) {
  PaletteEntry e;
  e.r = r;
  e.g = g;
  e.b = b;
  e.ok = srgbToOklab(r, g, b);
  return e;
}

int nearestIndex(int argb, const std::vector<PaletteEntry>& palette) {
  uint32_t p = static_cast<uint32_t>(argb);
  int r = (p >> 16) & 0xFF;
  int g = (p >> 8) & 0xFF;
  int b = p & 0xFF;
  OkLab q = srgbToOklab(r, g, b);

  int best = 0;
  float bestDist = std::numeric_limits<float>::max();
  for (size_t i = 0; i < palette.size(); i++) {
    float dL = q.L - palette[i].ok.L;
    float da = q.a - palette[i].ok.a;
    float db = q.b - palette[i].ok.b;
    float dist = dL * dL + da * da + db * db;
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

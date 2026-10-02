#include "lego/image_sampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace lego {
namespace {

int clampBlockSize(int blockSize) {
  if (blockSize < MIN_BLOCK_SIZE) {
    return MIN_BLOCK_SIZE;
  }
  if (blockSize > MAX_BLOCK_SIZE) {
    return MAX_BLOCK_SIZE;
  }
  return blockSize;
}

int averageRegion(const int* pixels, int sw, int x0, int x1, int y0, int y1) {
  int64_t sumR = 0;
  int64_t sumG = 0;
  int64_t sumB = 0;
  int count = (y1 - y0) * (x1 - x0);
  for (int y = y0; y < y1; y++) {
    int row = y * sw;
    for (int x = x0; x < x1; x++) {
      uint32_t p = static_cast<uint32_t>(pixels[row + x]);
      sumR += (p >> 16) & 0xFF;
      sumG += (p >> 8) & 0xFF;
      sumB += p & 0xFF;
    }
  }
  return (0xFF << 24) | (static_cast<int>(sumR / count) << 16) |
         (static_cast<int>(sumG / count) << 8) | static_cast<int>(sumB / count);
}

ImageBuffer averageBlocks(const int* src, int sw, int tw, int th, int blockW, int blockH) {
  ImageBuffer out;
  out.width = tw;
  out.height = th;
  out.argb.resize(static_cast<size_t>(tw) * th);
  for (int oy = 0; oy < th; oy++) {
    int y0 = oy * blockH;
    int y1 = y0 + blockH;
    for (int ox = 0; ox < tw; ox++) {
      int x0 = ox * blockW;
      int x1 = x0 + blockW;
      out.argb[oy * tw + ox] = averageRegion(src, sw, x0, x1, y0, y1);
    }
  }
  return out;
}

}  // namespace

int studCountFor(int srcWidth, int srcHeight, int blockSize) {
  if (blockSize < 1) {
    throw std::invalid_argument("blockSize must be >= 1");
  }
  int tw = std::max(1, srcWidth / blockSize);
  int th = std::max(1, srcHeight / blockSize);
  return tw * th;
}

int blockSizeForTargetStuds(int srcWidth, int srcHeight, int targetStuds) {
  if (srcWidth < 1 || srcHeight < 1) {
    throw std::invalid_argument("image dimensions must be positive");
  }
  if (targetStuds < 1) {
    throw std::invalid_argument("targetStuds must be >= 1");
  }
  int64_t pixels = static_cast<int64_t>(srcWidth) * srcHeight;
  int guess = static_cast<int>(std::llround(std::sqrt(static_cast<double>(pixels) / targetStuds)));
  guess = clampBlockSize(guess);

  int best = guess;
  int64_t bestErr = INT64_MAX;
  int b0 = std::max(MIN_BLOCK_SIZE, guess - 4);
  int b1 = std::min(MAX_BLOCK_SIZE, guess + 4);
  for (int b = b0; b <= b1; b++) {
    int64_t err = std::llabs(static_cast<int64_t>(studCountFor(srcWidth, srcHeight, b)) - targetStuds);
    if (err < bestErr || (err == bestErr && b < best)) {
      bestErr = err;
      best = b;
    }
  }
  return best;
}

int gridHeightFor(int srcWidth, int srcHeight, int targetStudWidth) {
  return std::max(1, static_cast<int>(std::lround(
                         static_cast<float>(srcHeight) * targetStudWidth / srcWidth)));
}

ImageBuffer toStudGridByBlockSize(const int* src, int sw, int sh, int blockSize) {
  if (blockSize < 1) {
    throw std::invalid_argument("blockSize must be >= 1");
  }
  int tw = std::max(1, sw / blockSize);
  int th = std::max(1, sh / blockSize);
  return averageBlocks(src, sw, tw, th, blockSize, blockSize);
}

ImageBuffer toStudGrid(const int* src, int sw, int sh, int targetStudWidth) {
  int tw = std::min(targetStudWidth, sw);
  int th = std::min(gridHeightFor(sw, sh, targetStudWidth), sh);
  if (tw < 1) {
    tw = 1;
  }
  if (th < 1) {
    th = 1;
  }
  ImageBuffer out;
  out.width = tw;
  out.height = th;
  out.argb.resize(static_cast<size_t>(tw) * th);
  for (int oy = 0; oy < th; oy++) {
    int y0 = static_cast<int>((static_cast<int64_t>(oy) * sh) / th);
    int y1 = std::max(y0 + 1, static_cast<int>((static_cast<int64_t>(oy + 1) * sh) / th));
    for (int ox = 0; ox < tw; ox++) {
      int x0 = static_cast<int>((static_cast<int64_t>(ox) * sw) / tw);
      int x1 = std::max(x0 + 1, static_cast<int>((static_cast<int64_t>(ox + 1) * sw) / tw));
      out.argb[oy * tw + ox] = averageRegion(src, sw, x0, x1, y0, y1);
    }
  }
  return out;
}

}  // namespace lego

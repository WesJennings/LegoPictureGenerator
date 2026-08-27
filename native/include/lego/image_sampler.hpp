#pragma once

#include "lego/types.hpp"

#include <cstdint>
#include <vector>

namespace lego {

struct ImageBuffer {
  int width = 0;
  int height = 0;
  std::vector<int> argb;
};

int studCountFor(int srcWidth, int srcHeight, int blockSize);
int blockSizeForTargetStuds(int srcWidth, int srcHeight, int targetStuds);
int gridHeightFor(int srcWidth, int srcHeight, int targetStudWidth);

ImageBuffer toStudGridByBlockSize(const int* src, int sw, int sh, int blockSize);
ImageBuffer toStudGrid(const int* src, int sw, int sh, int targetStudWidth);

}  // namespace lego

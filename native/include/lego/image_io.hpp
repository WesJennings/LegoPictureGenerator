#pragma once

#include "lego/image_sampler.hpp"

#include <string>

namespace lego {

struct DecodedImage {
  int width = 0;
  int height = 0;
  std::vector<int> argb;  // 0xAARRGGBB, size width*height
};

DecodedImage loadImageArgb(const std::string& path);
void writePngArgb(const std::string& path, const int* argb, int width, int height);
void writePngArgb(const std::string& path, const ImageBuffer& img);

}  // namespace lego

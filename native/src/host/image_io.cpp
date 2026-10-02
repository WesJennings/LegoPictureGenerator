#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "lego/image_io.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

namespace lego {

DecodedImage loadImageArgb(const std::string& path) {
  int w = 0;
  int h = 0;
  int comp = 0;
  struct StbiFree {
    void operator()(stbi_uc* p) const {
      if (p) {
        stbi_image_free(p);
      }
    }
  };
  std::unique_ptr<stbi_uc, StbiFree> data(stbi_load(path.c_str(), &w, &h, &comp, 4));
  if (!data) {
    throw std::runtime_error("Could not decode input image");
  }
  if (w < 1 || h < 1) {
    throw std::runtime_error("Could not decode input image");
  }
  const int64_t pixels = static_cast<int64_t>(w) * h;
  if (pixels > MAX_PIXELS) {
    throw std::runtime_error("Image is too large (max 25 megapixels)");
  }
  DecodedImage out;
  out.width = w;
  out.height = h;
  out.argb.resize(static_cast<size_t>(pixels));
  for (int64_t i = 0; i < pixels; i++) {
    int r = data.get()[i * 4 + 0];
    int g = data.get()[i * 4 + 1];
    int b = data.get()[i * 4 + 2];
    int a = data.get()[i * 4 + 3];
    out.argb[static_cast<size_t>(i)] = (a << 24) | (r << 16) | (g << 8) | b;
  }
  return out;
}

void writePngArgb(const std::string& path, const int* argb, int width, int height) {
  if (width < 1 || height < 1) {
    throw std::runtime_error("invalid png size");
  }
  const int64_t pixels = static_cast<int64_t>(width) * height;
  std::vector<unsigned char> rgba(static_cast<size_t>(pixels) * 4);
  for (int64_t i = 0; i < pixels; i++) {
    uint32_t p = static_cast<uint32_t>(argb[i]);
    rgba[i * 4 + 0] = static_cast<unsigned char>((p >> 16) & 0xFF);
    rgba[i * 4 + 1] = static_cast<unsigned char>((p >> 8) & 0xFF);
    rgba[i * 4 + 2] = static_cast<unsigned char>(p & 0xFF);
    rgba[i * 4 + 3] = static_cast<unsigned char>((p >> 24) & 0xFF);
  }
  if (!stbi_write_png(path.c_str(), width, height, 4, rgba.data(), width * 4)) {
    throw std::runtime_error("No PNG writer available for " + path);
  }
}

void writePngArgb(const std::string& path, const ImageBuffer& img) {
  writePngArgb(path, img.argb.data(), img.width, img.height);
}

}  // namespace lego

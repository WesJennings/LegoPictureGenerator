#include "lego/upload_validator.hpp"

#include "lego/types.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

namespace lego {
namespace {

uint32_t be32(const unsigned char* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint16_t be16(const unsigned char* p) {
  return static_cast<uint16_t>((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

}  // namespace

bool isPngSignature(const unsigned char* b, size_t n) {
  return n >= 4 && b[0] == 0x89 && b[1] == 0x50 && b[2] == 0x4E && b[3] == 0x47;
}

bool isJpegSignature(const unsigned char* b, size_t n) {
  return n >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF;
}

std::pair<int, int> pngDimensions(const unsigned char* data, size_t n) {
  static const unsigned char sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (n < 24) {
    throw InvalidUpload("Unreadable image file");
  }
  for (int i = 0; i < 8; i++) {
    if (data[i] != sig[i]) {
      throw InvalidUpload("Unreadable image file");
    }
  }
  if (data[12] != 'I' || data[13] != 'H' || data[14] != 'D' || data[15] != 'R') {
    throw InvalidUpload("Unreadable image file");
  }
  uint32_t w = be32(data + 16);
  uint32_t h = be32(data + 20);
  if (w < 1 || h < 1 || w > 1'000'000u || h > 1'000'000u) {
    throw InvalidUpload("Image has invalid dimensions");
  }
  return {static_cast<int>(w), static_cast<int>(h)};
}

std::pair<int, int> jpegDimensions(const unsigned char* data, size_t n) {
  if (n < 4 || data[0] != 0xFF || data[1] != 0xD8) {
    throw InvalidUpload("Unreadable image file");
  }
  size_t i = 2;
  while (i + 3 < n) {
    if (data[i] != 0xFF) {
      i++;
      continue;
    }
    while (i < n && data[i] == 0xFF) {
      i++;
    }
    if (i >= n) {
      break;
    }
    unsigned char marker = data[i++];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8)) {
      continue;
    }
    if (i + 1 >= n) {
      throw InvalidUpload("Unreadable image file");
    }
    uint16_t len = be16(data + i);
    if (len < 2 || i + len > n) {
      throw InvalidUpload("Unreadable image file");
    }
    bool sof = (marker >= 0xC0 && marker <= 0xCF) && marker != 0xC4 && marker != 0xC8 &&
               marker != 0xCC;
    if (sof) {
      if (len < 7 || i + 7 > n) {
        throw InvalidUpload("Unreadable image file");
      }
      uint16_t h = be16(data + i + 3);
      uint16_t w = be16(data + i + 5);
      if (w < 1 || h < 1) {
        throw InvalidUpload("Image has invalid dimensions");
      }
      return {static_cast<int>(w), static_cast<int>(h)};
    }
    i += len;
  }
  throw InvalidUpload("Unreadable image file");
}

void validateUpload(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    throw InvalidUpload("Unreadable image file");
  }
  auto size = in.tellg();
  if (size < 0 || static_cast<int64_t>(size) > MAX_UPLOAD_BYTES) {
    throw InvalidUpload("Upload exceeds 25 MB limit");
  }
  in.seekg(0, std::ios::beg);
  std::vector<unsigned char> head(static_cast<size_t>(
      std::min<int64_t>(static_cast<int64_t>(size), 1 << 20)));
  in.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
  head.resize(static_cast<size_t>(in.gcount()));
  if (head.size() < 4) {
    throw InvalidUpload("Only PNG and JPEG images are supported");
  }
  int w = 0;
  int h = 0;
  if (isPngSignature(head.data(), head.size())) {
    if (head.size() < 24) {
      throw InvalidUpload("Unreadable image file");
    }
    auto wh = pngDimensions(head.data(), head.size());
    w = wh.first;
    h = wh.second;
  } else if (isJpegSignature(head.data(), head.size())) {
    if (static_cast<int64_t>(head.size()) < static_cast<int64_t>(size) &&
        static_cast<int64_t>(size) <= 4 * 1024 * 1024) {
      in.clear();
      in.seekg(0);
      head.resize(static_cast<size_t>(size));
      in.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
      head.resize(static_cast<size_t>(in.gcount()));
    }
    auto wh = jpegDimensions(head.data(), head.size());
    w = wh.first;
    h = wh.second;
  } else {
    throw InvalidUpload("Only PNG and JPEG images are supported");
  }
  if (static_cast<int64_t>(w) * static_cast<int64_t>(h) > MAX_PIXELS) {
    throw InvalidUpload("Image is too large (max 25 megapixels)");
  }
}

}  // namespace lego

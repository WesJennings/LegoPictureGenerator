#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace lego {

class InvalidUpload : public std::exception {
 public:
  explicit InvalidUpload(std::string message) : message_(std::move(message)) {}
  const char* what() const noexcept override { return message_.c_str(); }

 private:
  std::string message_;
};

/** Magic bytes + header dimensions. Does not decode pixels (zip-bomb safe). */
void validateUpload(const std::string& path);

bool isPngSignature(const unsigned char* b, size_t n);
bool isJpegSignature(const unsigned char* b, size_t n);
std::pair<int, int> pngDimensions(const unsigned char* data, size_t n);
std::pair<int, int> jpegDimensions(const unsigned char* data, size_t n);

}  // namespace lego

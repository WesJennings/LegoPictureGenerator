#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace lego {

struct PlateSize {
  std::string partNum;
  int w = 0;
  int h = 0;
  int area() const { return w * h; }
};

struct PlacedPart {
  std::string partNum;
  int colorId = 0;
  std::string colorName;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

struct PackResult {
  std::string modeName;
  std::vector<PlacedPart> placed;
  int64_t elapsedMs = 0;
  std::string status = "ok";
  int pieceCount() const { return static_cast<int>(placed.size()); }
};

struct LegoElement {
  std::string partNum;
  int colorId = 0;
  std::string colorName;
  std::string rgbHex;
  int r = 0;
  int g = 0;
  int b = 0;
  bool present = true;

  int toArgb() const { return (0xFF << 24) | (r << 16) | (g << 8) | b; }
};

/** Per-color footprint lists, already largest-first (same as Java PlateCatalog). */
class PlateCatalog {
 public:
  std::unordered_map<int, std::vector<PlateSize>> byColor;

  const std::vector<PlateSize>& footprintsForColor(int colorId) const {
    static const std::vector<PlateSize> kEmpty;
    auto it = byColor.find(colorId);
    return it == byColor.end() ? kEmpty : it->second;
  }
};

inline constexpr int MIN_BLOCK_SIZE = 8;
inline constexpr int MAX_BLOCK_SIZE = 256;

inline int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace lego

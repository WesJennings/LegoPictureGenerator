#pragma once

#include "lego/types.hpp"

#include <map>
#include <string>
#include <vector>

namespace lego {

struct BomRow {
  std::string partNum;
  int w = 0;
  int h = 0;
  int colorId = 0;
  std::string colorName;
  int count = 0;
};

struct ColorCountRow {
  int colorId = 0;
  std::string colorName;
  std::string rgbHex;
  int count = 0;
};

std::vector<std::string> formatBom(const PackResult& result);
std::vector<std::string> formatStudBom(const std::vector<BomRow>& studBom);
std::vector<std::string> formatColorCounts(
    const std::map<int, int>& colorCounts,
    const std::map<int, LegoElement>& colorSamples);

std::vector<BomRow> bomRows(const PackResult& result);
std::vector<BomRow> studBomRows(const std::map<int, int>& colorCounts,
                                const std::map<int, LegoElement>& colorSamples);
std::vector<ColorCountRow> colorCountRows(const std::map<int, int>& colorCounts,
                                          const std::map<int, LegoElement>& colorSamples);

std::string capitalize(const std::string& s);
void writeLines(const std::string& path, const std::vector<std::string>& lines);

}  // namespace lego

#pragma once

#include "lego/color_matcher.hpp"
#include "lego/types.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lego {

struct Catalog {
  std::vector<LegoElement> palette;
  PlateCatalog plates;
  std::string dbPath;
};

bool isTransparentFlag(const std::string& isTrans);
LegoElement makeElement(const std::string& partNum, int colorId,
                        const std::string& colorName, const std::string& rgbHex);
Catalog loadCatalog(const std::string& dbPath);

std::vector<PaletteEntry> paletteRgb(const std::vector<LegoElement>& palette);

}  // namespace lego

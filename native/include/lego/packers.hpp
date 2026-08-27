#pragma once

#include "lego/types.hpp"

#include <string>
#include <utility>
#include <vector>

namespace lego {

using StudGrid = std::vector<std::vector<LegoElement>>;

bool fits(const StudGrid& studs, const std::vector<std::vector<char>>& covered,
          int x, int y, int pw, int ph, int colorId, int w, int h);
void mark(std::vector<std::vector<char>>& covered, int x, int y, int pw, int ph);

std::vector<std::pair<int, int>> flood(const StudGrid& studs,
                                       std::vector<std::vector<char>>& visited,
                                       int sx, int sy, int colorId, int w, int h);

std::vector<PlacedPart> greedyComponent(const PlateCatalog& catalog,
                                        const StudGrid& studs,
                                        const std::vector<std::pair<int, int>>& cells,
                                        int w, int h);

PackResult packGreedy(const PlateCatalog& catalog, const StudGrid& studs);
PackResult packRle(const PlateCatalog& catalog, const StudGrid& studs);
PackResult packComponent(const PlateCatalog& catalog, const StudGrid& studs);
PackResult packIlp(const PlateCatalog& catalog, const StudGrid& studs);
PackResult packDlx(const PlateCatalog& catalog, const StudGrid& studs);
PackResult packAnneal(const PlateCatalog& catalog, const StudGrid& studs);

PackResult pack(const std::string& mode, const PlateCatalog& catalog, const StudGrid& studs);

}  // namespace lego

#pragma once

#include "lego/types.hpp"

#include <cstdint>
#include <vector>

namespace lego {

std::vector<int> renderStuds(const int* gridArgb, int cols, int rows, int studSizePx);
std::vector<int> renderPacked(const int* gridArgb, int cols, int rows, int studSizePx,
                              const std::vector<PlacedPart>& placed);

}  // namespace lego

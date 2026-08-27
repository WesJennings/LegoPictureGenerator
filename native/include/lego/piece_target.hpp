#pragma once

#include <functional>

namespace lego {

struct PieceSearchResult {
  int blockSize = 0;
  int studTargetUsed = 0;
  int bestPieceCount = 0;
  int attempts = 0;
};

PieceSearchResult solvePieceTarget(
    int srcWidth,
    int srcHeight,
    int targetPieces,
    const std::function<int(int blockSize)>& probe);

}  // namespace lego

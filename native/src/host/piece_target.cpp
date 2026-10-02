#include "lego/piece_target.hpp"

#include "lego/image_sampler.hpp"
#include "lego/types.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lego {

PieceSearchResult solvePieceTarget(
    int srcWidth,
    int srcHeight,
    int targetPieces,
    const std::function<int(int blockSize)>& probe) {
  if (targetPieces < 1) {
    throw std::invalid_argument("targetPieces must be >= 1");
  }
  int lo = MIN_TARGET_STUDS;
  int hi = MAX_TARGET_STUDS;
  int mid = static_cast<int>(std::llround(targetPieces / PIECE_SEARCH_RATIO));
  mid = std::max(lo, std::min(hi, mid));

  int bestBlock = blockSizeForTargetStuds(srcWidth, srcHeight, mid);
  int bestPieces = -1;
  int bestErr = 2147483647;
  int bestStudTarget = mid;
  int attempts = 0;

  for (int i = 0; i < MAX_PIECE_PROBES && lo <= hi; i++) {
    attempts++;
    int blockSize = blockSizeForTargetStuds(srcWidth, srcHeight, mid);
    int pieces = probe(blockSize);
    int err = std::abs(pieces - targetPieces);
    if (err < bestErr || (err == bestErr && (bestPieces < 0 || pieces < bestPieces))) {
      bestErr = err;
      bestPieces = pieces;
      bestBlock = blockSize;
      bestStudTarget = mid;
    }
    double rel = targetPieces == 0 ? 0.0 : static_cast<double>(err) / targetPieces;
    if (rel <= PIECE_SEARCH_TOLERANCE) {
      break;
    }
    if (pieces > targetPieces) {
      hi = mid - 1;
    } else {
      lo = mid + 1;
    }
    if (lo > hi) {
      break;
    }
    mid = (lo + hi) / 2;
  }
  return PieceSearchResult{bestBlock, bestStudTarget, bestPieces, attempts};
}

}  // namespace lego

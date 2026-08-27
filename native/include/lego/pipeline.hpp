#pragma once

#include "lego/catalog.hpp"
#include "lego/image_io.hpp"
#include "lego/types.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace lego {

using ProgressFn = std::function<void(const std::string& stage)>;

void validateJobConfig(const JobConfig& cfg);

/** Canonical lowercase pack mode, or throw invalid_argument like PackMode.fromModeName. */
std::string canonicalPackMode(const std::string& name);

/** HTTP default is greedy when raw is blank. CLI requires explicit modes when provided. */
std::vector<std::string> parsePackModes(const std::string& raw, bool defaultGreedyIfBlank);

/** PackMode.values() order intersected with the requested set. */
std::vector<std::string> orderedPackModes(const std::vector<std::string>& requested);

int probePieceCount(const DecodedImage& src, const Catalog& catalog, int blockSize,
                    const std::string& mode, const std::atomic<bool>* cancel = nullptr);

PipelineResult runPipeline(const Catalog& catalog, const JobConfig& cfg,
                           ProgressFn progress = nullptr,
                           const std::atomic<bool>* cancel = nullptr);

}  // namespace lego

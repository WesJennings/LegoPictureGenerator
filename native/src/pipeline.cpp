#include "lego/pipeline.hpp"

#include "lego/color_matcher.hpp"
#include "lego/image_io.hpp"
#include "lego/image_sampler.hpp"
#include "lego/packers.hpp"
#include "lego/renderer.hpp"
#include "lego/text.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <unordered_set>

#include "json.hpp"

namespace lego {
namespace {

using json = nlohmann::ordered_json;

void checkInterrupted(const std::atomic<bool>* cancel) {
  if (cancel && cancel->load()) {
    throw std::runtime_error("pipeline interrupted");
  }
}

std::string toLowerCopy(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

json placedToJson(const std::vector<PlacedPart>& placed) {
  json arr = json::array();
  for (const auto& p : placed) {
    json o;
    o["partNum"] = p.partNum;
    o["colorId"] = p.colorId;
    o["colorName"] = p.colorName;
    o["x"] = p.x;
    o["y"] = p.y;
    o["w"] = p.w;
    o["h"] = p.h;
    arr.push_back(std::move(o));
  }
  return arr;
}

StudGrid studGridFromMatch(const MatchResult& matched, int w, int h,
                           const std::vector<LegoElement>& palette,
                           std::map<int, int>& colorCounts,
                           std::map<int, LegoElement>& colorSamples) {
  StudGrid studs(static_cast<size_t>(h), std::vector<LegoElement>(static_cast<size_t>(w)));
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int idx = matched.paletteIndex[static_cast<size_t>(y * w + x)];
      if (idx < 0 || idx >= static_cast<int>(palette.size())) {
        throw std::runtime_error("color match index out of range");
      }
      const LegoElement& el = palette[static_cast<size_t>(idx)];
      studs[static_cast<size_t>(y)][static_cast<size_t>(x)] = el;
      colorCounts[el.colorId] += 1;
      colorSamples.emplace(el.colorId, el);
    }
  }
  return studs;
}

PipelineBomRow toPipelineBom(const BomRow& row) {
  return PipelineBomRow{row.partNum, row.w, row.h, row.colorId, row.colorName, row.count};
}

}  // namespace

void validateJobConfig(const JobConfig& cfg) {
  if (cfg.targetStudWidth > 0) {
    if (cfg.targetStudWidth < 1 || cfg.targetStudWidth > 512) {
      throw std::invalid_argument("targetStudWidth must be in [1, 512]");
    }
  } else if (cfg.blockSize < MIN_BLOCK_SIZE || cfg.blockSize > MAX_BLOCK_SIZE) {
    throw std::invalid_argument("blockSize must be in [8, 256]");
  }
  if (cfg.renderStudSizePx < 4) {
    throw std::invalid_argument("renderStudSizePx must be >= 4");
  }
  if (cfg.modes.empty()) {
    throw std::invalid_argument("at least one pack mode is required");
  }
}

std::string canonicalPackMode(const std::string& name) {
  std::string v = toLowerCopy(name);
  for (int i = 0; i < kPackModeCount; i++) {
    if (v == kPackModeOrder[i]) {
      return v;
    }
  }
  throw std::invalid_argument("Unknown pack mode: " + name);
}

std::vector<std::string> parsePackModes(const std::string& raw, bool defaultGreedyIfBlank) {
  std::string trimmed = raw;
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), notSpace));
  trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), notSpace).base(), trimmed.end());
  if (trimmed.empty()) {
    if (defaultGreedyIfBlank) {
      return {"greedy"};
    }
    throw std::invalid_argument("at least one pack mode is required");
  }
  std::vector<std::string> modes;
  std::unordered_set<std::string> seen;
  size_t start = 0;
  while (start <= trimmed.size()) {
    size_t comma = trimmed.find(',', start);
    std::string part = comma == std::string::npos ? trimmed.substr(start)
                                                  : trimmed.substr(start, comma - start);
    part.erase(part.begin(), std::find_if(part.begin(), part.end(), notSpace));
    part.erase(std::find_if(part.rbegin(), part.rend(), notSpace).base(), part.end());
    std::string canon = canonicalPackMode(part);
    if (seen.insert(canon).second) {
      modes.push_back(canon);
    }
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
  return modes;
}

std::vector<std::string> orderedPackModes(const std::vector<std::string>& requested) {
  std::unordered_set<std::string> want;
  for (const auto& m : requested) {
    want.insert(canonicalPackMode(m));
  }
  std::vector<std::string> out;
  for (int i = 0; i < kPackModeCount; i++) {
    if (want.count(kPackModeOrder[i])) {
      out.push_back(kPackModeOrder[i]);
    }
  }
  return out;
}

int probePieceCount(const DecodedImage& src, const Catalog& catalog, int blockSize,
                    const std::string& mode, const std::atomic<bool>* cancel) {
  ImageBuffer grid = toStudGridByBlockSize(src.argb.data(), src.width, src.height, blockSize);
  checkInterrupted(cancel);
  auto pal = paletteRgb(catalog.palette);
  MatchResult matched = matchImage(grid.argb.data(), grid.width, grid.height, pal);
  std::map<int, int> colorCounts;
  std::map<int, LegoElement> colorSamples;
  StudGrid studs = studGridFromMatch(matched, grid.width, grid.height, catalog.palette,
                                    colorCounts, colorSamples);
  checkInterrupted(cancel);
  PackResult result = pack(mode, catalog.plates, studs);
  checkInterrupted(cancel);
  return result.pieceCount();
}

PipelineResult runPipeline(const Catalog& catalog, const JobConfig& cfg, ProgressFn progress,
                           const std::atomic<bool>* cancel) {
  validateJobConfig(cfg);
  std::filesystem::create_directories(cfg.outputDirectory);
  auto report = [&](const std::string& stage) {
    if (progress) {
      progress(stage);
    }
  };

  report("PREPROCESSING");
  DecodedImage src = loadImageArgb(cfg.inputPath);
  checkInterrupted(cancel);

  ImageBuffer grid = cfg.usesStudWidthSampling()
                         ? toStudGrid(src.argb.data(), src.width, src.height, cfg.targetStudWidth)
                         : toStudGridByBlockSize(src.argb.data(), src.width, src.height,
                                                 cfg.blockSize);
  checkInterrupted(cancel);

  report("MATCHING");
  auto pal = paletteRgb(catalog.palette);
  MatchResult matched = matchImage(grid.argb.data(), grid.width, grid.height, pal);
  std::map<int, int> colorCounts;
  std::map<int, LegoElement> colorSamples;
  StudGrid studs = studGridFromMatch(matched, grid.width, grid.height, catalog.palette,
                                    colorCounts, colorSamples);

  writePngArgb(cfg.outputDirectory + "/matched.png", matched.matchedArgb.data(), grid.width,
               grid.height);
  std::vector<std::pair<std::string, std::string>> artifacts;
  artifacts.emplace_back("matched", "matched.png");

  writeLines(cfg.outputDirectory + "/color-counts.txt",
             formatColorCounts(colorCounts, colorSamples));
  artifacts.emplace_back("colorCounts", "color-counts.txt");

  std::vector<PipelineBomRow> studBom;
  if (cfg.includeStudBom) {
    auto rows = studBomRows(colorCounts, colorSamples);
    writeLines(cfg.outputDirectory + "/bom-studs.txt", formatStudBom(rows));
    artifacts.emplace_back("bomStuds", "bom-studs.txt");
    for (const auto& row : rows) {
      studBom.push_back(toPipelineBom(row));
    }
  }
  checkInterrupted(cancel);

  report("PACKING");
  std::vector<std::string> modes = orderedPackModes(cfg.modes);
  std::vector<std::pair<std::string, PackResult>> packResults;
  for (const auto& mode : modes) {
    packResults.emplace_back(mode, pack(mode, catalog.plates, studs));
    checkInterrupted(cancel);
  }

  report("RENDERING");
  int rw = grid.width * cfg.renderStudSizePx;
  int rh = grid.height * cfg.renderStudSizePx;
  std::vector<int> legoStuds =
      renderStuds(matched.matchedArgb.data(), grid.width, grid.height, cfg.renderStudSizePx);
  writePngArgb(cfg.outputDirectory + "/lego-studs.png", legoStuds.data(), rw, rh);
  artifacts.emplace_back("legoStuds", "lego-studs.png");

  std::vector<PipelineModeResult> modeResults;
  std::vector<PipelineColorCountRow> colorRows;
  for (const auto& row : colorCountRows(colorCounts, colorSamples)) {
    colorRows.push_back(
        PipelineColorCountRow{row.colorId, row.colorName, row.rgbHex, row.count});
  }

  for (auto& item : packResults) {
    const std::string& mode = item.first;
    PackResult& result = item.second;
    std::vector<int> packed = renderPacked(matched.matchedArgb.data(), grid.width, grid.height,
                                           cfg.renderStudSizePx, result.placed);
    std::string renderName = "lego-" + mode + ".png";
    writePngArgb(cfg.outputDirectory + "/" + renderName, packed.data(), rw, rh);
    artifacts.emplace_back("lego" + capitalize(mode), renderName);

    std::string bomName = "bom-" + mode + ".txt";
    writeLines(cfg.outputDirectory + "/" + bomName, formatBom(result));
    artifacts.emplace_back("bom" + capitalize(mode), bomName);

    std::string placementsName = "placements-" + mode + ".json";
    {
      std::ofstream out(cfg.outputDirectory + "/" + placementsName);
      if (!out) {
        throw std::runtime_error("could not write " + placementsName);
      }
      out << placedToJson(result.placed).dump(2);
    }
    artifacts.emplace_back("placements" + capitalize(mode), placementsName);

    PipelineModeResult mr;
    mr.mode = mode;
    mr.status = result.status;
    mr.elapsedMs = result.elapsedMs;
    mr.pieceCount = result.pieceCount();
    for (const auto& row : bomRows(result)) {
      mr.bom.push_back(toPipelineBom(row));
    }
    modeResults.push_back(std::move(mr));
    checkInterrupted(cancel);
  }

  PipelineResult out;
  out.gridWidth = grid.width;
  out.gridHeight = grid.height;
  out.totalStuds = grid.width * grid.height;
  out.colorCounts = std::move(colorRows);
  out.studBom = std::move(studBom);
  out.modeResults = std::move(modeResults);
  out.artifacts = std::move(artifacts);
  return out;
}

}  // namespace lego

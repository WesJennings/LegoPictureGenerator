#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
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

inline constexpr int DEFAULT_BLOCK_SIZE = 80;
inline constexpr int MIN_BLOCK_SIZE = 8;
inline constexpr int MAX_BLOCK_SIZE = 256;
inline constexpr int DEFAULT_RENDER_STUD_PX = 24;
inline constexpr int CLASSIC_STUD_WIDTH = 54;
inline constexpr int DEFAULT_TARGET_STUDS = 4000;
inline constexpr int MIN_TARGET_STUDS = 400;
inline constexpr int MAX_TARGET_STUDS = 12'000;
inline constexpr int DEFAULT_TARGET_PIECES = 800;
inline constexpr int MIN_TARGET_PIECES = 150;
inline constexpr int MAX_TARGET_PIECES = 4000;
inline constexpr double PIECE_SEARCH_RATIO = 0.28;
inline constexpr double PIECE_SEARCH_TOLERANCE = 0.10;
inline constexpr int MAX_PIECE_PROBES = 5;
inline constexpr const char* SIZING_FIXED = "fixed";
inline constexpr const char* SIZING_AUTO = "auto";
inline constexpr const char* SIZING_PIECES = "pieces";
inline constexpr const char* DEFAULT_STUD_PART = "3024";
inline constexpr int64_t MAX_UPLOAD_BYTES = 25LL * 1024 * 1024;
inline constexpr int64_t MAX_PIXELS = 25'000'000LL;
inline constexpr int QUEUE_CAPACITY = 8;
inline constexpr int64_t JOB_TIMEOUT_MS = 120'000;
inline constexpr int64_t PER_MODE_BUDGET_MS = 90'000;
inline constexpr int64_t MAX_JOB_TIMEOUT_MS = 600'000;
inline constexpr int MAX_JOBS = 50;
inline constexpr int64_t MAX_JOB_AGE_MS = 7LL * 24 * 60 * 60 * 1000;
inline constexpr int64_t MAX_TOTAL_BYTES = 2LL * 1024 * 1024 * 1024;

inline const char* kPackModeOrder[] = {
    "greedy", "ilp", "rle", "component", "dlx", "anneal"};
inline constexpr int kPackModeCount = 6;

inline int64_t nowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/** Everything one pipeline run needs. No global state, no fixed paths. */
struct JobConfig {
  std::string inputPath;
  std::string outputDirectory;
  int blockSize = DEFAULT_BLOCK_SIZE;
  /** When > 0, sample with toStudGrid(width) instead of block size. */
  int targetStudWidth = 0;
  int renderStudSizePx = DEFAULT_RENDER_STUD_PX;
  std::vector<std::string> modes;
  bool includeStudBom = false;

  bool usesStudWidthSampling() const { return targetStudWidth > 0; }
};

struct PipelineBomRow {
  std::string partNum;
  int w = 0;
  int h = 0;
  int colorId = 0;
  std::string colorName;
  int count = 0;
};

struct PipelineColorCountRow {
  int colorId = 0;
  std::string colorName;
  std::string rgbHex;
  int count = 0;
};

struct PipelineModeResult {
  std::string mode;
  std::string status;
  int64_t elapsedMs = 0;
  int pieceCount = 0;
  std::vector<PipelineBomRow> bom;
};

struct PipelineResult {
  int gridWidth = 0;
  int gridHeight = 0;
  int totalStuds = 0;
  std::vector<PipelineColorCountRow> colorCounts;
  std::vector<PipelineBomRow> studBom;
  std::vector<PipelineModeResult> modeResults;
  /** Logical artifact name -> filename inside the job directory. */
  std::vector<std::pair<std::string, std::string>> artifacts;
};

inline bool isTerminalStatus(const std::string& status) {
  return status == "COMPLETE" || status == "FAILED";
}

}  // namespace lego

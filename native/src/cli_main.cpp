#include "lego/catalog.hpp"
#include "lego/config.hpp"
#include "lego/pipeline.hpp"
#include "lego/types.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  try {
    std::string input = argc > 1 ? argv[1] : "samples/Jarvis.png";
    std::string outputDir = argc > 2 ? argv[2] : "runtime/cli";
    int blockSize = lego::DEFAULT_BLOCK_SIZE;
    if (argc > 3) {
      size_t idx = 0;
      blockSize = std::stoi(argv[3], &idx, 10);
      if (idx != std::string(argv[3]).size()) {
        throw std::invalid_argument("blockSize must be a number");
      }
    }
    std::vector<std::string> modes =
        argc > 4 ? lego::parsePackModes(argv[4], false) : std::vector<std::string>{"greedy"};

    const char* dbEnv = std::getenv("LEGO_DB_PATH");
    std::string dbPath = (dbEnv && dbEnv[0]) ? dbEnv : "data/bricks.db";
    lego::Catalog catalog = lego::loadCatalog(dbPath);

    lego::JobConfig cfg;
    cfg.inputPath = input;
    cfg.outputDirectory = outputDir;
    cfg.blockSize = blockSize;
    cfg.targetStudWidth = 0;
    cfg.renderStudSizePx = lego::DEFAULT_RENDER_STUD_PX;
    cfg.modes = modes;
    cfg.includeStudBom = false;

    lego::PipelineResult result = lego::runPipeline(
        catalog, cfg, [](const std::string& stage) { std::cout << "[stage] " << stage << std::endl; });

    std::cout << "Grid: " << result.gridWidth << " x " << result.gridHeight << " ("
              << result.totalStuds << " studs)" << std::endl;
    for (const auto& mode : result.modeResults) {
      std::cout << std::left << std::setw(10) << mode.mode << " pieces=" << std::setw(6)
                << mode.pieceCount << " time=" << std::setw(7) << mode.elapsedMs
                << "ms status=" << mode.status << std::endl;
    }
    std::cout << "Artifacts written to " << outputDir << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}

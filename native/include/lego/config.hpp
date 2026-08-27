#pragma once

#include <string>

namespace lego {

struct AppConfig {
  std::string dbPath = "data/bricks.db";
  std::string jobsPath = "runtime/jobs";
  int port = 8080;
  int workerCount = 1;
  std::string webDist = "web/dist";

  static AppConfig fromEnvironment();
};

}  // namespace lego

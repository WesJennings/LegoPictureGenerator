#include "lego/config.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace lego {
namespace {

std::string envOr(const char* name, const char* fallback) {
  const char* v = std::getenv(name);
  if (v == nullptr || v[0] == '\0') {
    return fallback;
  }
  return v;
}

int envInt(const char* name, int fallback) {
  const char* v = std::getenv(name);
  if (v == nullptr || v[0] == '\0') {
    return fallback;
  }
  try {
    size_t idx = 0;
    int n = std::stoi(v, &idx, 10);
    if (idx != std::string(v).size()) {
      throw std::invalid_argument("trailing junk");
    }
    return n;
  } catch (...) {
    throw std::invalid_argument(std::string("invalid integer for ") + name);
  }
}

}  // namespace

AppConfig AppConfig::fromEnvironment() {
  AppConfig cfg;
  cfg.dbPath = envOr("LEGO_DB_PATH", "data/bricks.db");
  cfg.jobsPath = envOr("LEGO_JOBS_PATH", "runtime/jobs");
  cfg.port = envInt("LEGO_PORT", 8080);
  cfg.workerCount = envInt("LEGO_WORKER_COUNT", 1);
  if (cfg.port < 1 || cfg.port > 65535) {
    throw std::invalid_argument("LEGO_PORT must be in [1, 65535]");
  }
  if (cfg.workerCount < 1 || cfg.workerCount > 64) {
    throw std::invalid_argument("LEGO_WORKER_COUNT must be in [1, 64]");
  }
  return cfg;
}

}  // namespace lego

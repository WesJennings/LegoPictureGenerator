#pragma once

#include <string>
#include <vector>

namespace lego {

struct AppConfig {
  std::string dbPath = "data/bricks.db";
  std::string jobsPath = "runtime/jobs";
  /** Interface to listen on. 127.0.0.1 for local use; 0.0.0.0 inside a container. */
  std::string bind = "127.0.0.1";
  int port = 8080;
  int workerCount = 1;
  std::string webDist = "web/dist";
  /** Host header values (without port) that are accepted. DNS-rebinding guard. */
  std::vector<std::string> allowedHosts = {"localhost", "127.0.0.1"};
  /** When true, client IP is read from CF-Connecting-IP / X-Forwarded-For. */
  bool trustProxy = false;
  /** Per-client-IP cap on job creation. 0 disables. */
  int uploadsPerMinute = 0;

  static AppConfig fromEnvironment();
};

/** Split "a, b,c" into {"a","b","c"}; trims, lowercases, drops empties. */
std::vector<std::string> parseHostList(const std::string& raw);

}  // namespace lego

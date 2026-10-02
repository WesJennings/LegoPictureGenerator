#include "lego/config.hpp"

#include <algorithm>
#include <cctype>
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

bool envBool(const char* name, bool fallback) {
  const char* v = std::getenv(name);
  if (v == nullptr || v[0] == '\0') {
    return fallback;
  }
  std::string s = v;
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (s == "1" || s == "true" || s == "yes" || s == "on") {
    return true;
  }
  if (s == "0" || s == "false" || s == "no" || s == "off") {
    return false;
  }
  throw std::invalid_argument(std::string("invalid boolean for ") + name);
}

}  // namespace

std::vector<std::string> parseHostList(const std::string& raw) {
  std::vector<std::string> out;
  std::string cur;
  auto flush = [&] {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    cur.erase(cur.begin(), std::find_if(cur.begin(), cur.end(), notSpace));
    cur.erase(std::find_if(cur.rbegin(), cur.rend(), notSpace).base(), cur.end());
    for (char& c : cur) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (!cur.empty()) {
      out.push_back(cur);
    }
    cur.clear();
  };
  for (char c : raw) {
    if (c == ',') {
      flush();
    } else {
      cur.push_back(c);
    }
  }
  flush();
  return out;
}

AppConfig AppConfig::fromEnvironment() {
  AppConfig cfg;
  cfg.dbPath = envOr("LEGO_DB_PATH", "data/bricks.db");
  cfg.jobsPath = envOr("LEGO_JOBS_PATH", "runtime/jobs");
  cfg.bind = envOr("LEGO_BIND", "127.0.0.1");
  cfg.port = envInt("LEGO_PORT", 8080);
  cfg.workerCount = envInt("LEGO_WORKER_COUNT", 1);
  cfg.webDist = envOr("LEGO_WEB_DIST", "web/dist");
  cfg.allowedHosts = parseHostList(envOr("LEGO_ALLOWED_HOSTS", "localhost,127.0.0.1"));
  cfg.trustProxy = envBool("LEGO_TRUST_PROXY", false);
  cfg.uploadsPerMinute = envInt("LEGO_UPLOADS_PER_MINUTE", 0);
  if (cfg.port < 1 || cfg.port > 65535) {
    throw std::invalid_argument("LEGO_PORT must be in [1, 65535]");
  }
  if (cfg.workerCount < 1 || cfg.workerCount > 64) {
    throw std::invalid_argument("LEGO_WORKER_COUNT must be in [1, 64]");
  }
  if (cfg.allowedHosts.empty()) {
    throw std::invalid_argument("LEGO_ALLOWED_HOSTS must list at least one host");
  }
  if (cfg.uploadsPerMinute < 0 || cfg.uploadsPerMinute > 10'000) {
    throw std::invalid_argument("LEGO_UPLOADS_PER_MINUTE must be in [0, 10000]");
  }
  return cfg;
}

}  // namespace lego

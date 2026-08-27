#include "lego/catalog.hpp"
#include "lego/config.hpp"
#include "lego/http_server.hpp"
#include "lego/jobs.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
std::atomic<lego::HttpServer*> gServer{nullptr};

void onSignal(int) {
  if (auto* s = gServer.load()) {
    s->stop();
  }
}
}  // namespace

int main() {
  try {
    lego::AppConfig cfg = lego::AppConfig::fromEnvironment();
    lego::Catalog catalog = lego::loadCatalog(cfg.dbPath);
    auto repo = std::make_shared<lego::FileJobRepository>(cfg.jobsPath);
    repo->recoverInterruptedJobs();
    repo->enforceRetention();
    lego::JobService jobs(repo, catalog, cfg.workerCount);
    lego::HttpServer http(jobs, *repo, catalog, cfg.webDist);
    gServer = &http;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::cerr << "Lego Picture Generator listening on http://127.0.0.1:" << cfg.port
              << std::endl;
    if (!http.listen("127.0.0.1", cfg.port)) {
      std::cerr << "Failed to bind 127.0.0.1:" << cfg.port << std::endl;
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}

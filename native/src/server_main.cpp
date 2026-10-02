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
    lego::HttpOptions opts;
    opts.allowedHosts = cfg.allowedHosts;
    opts.trustProxy = cfg.trustProxy;
    opts.uploadsPerMinute = cfg.uploadsPerMinute;
    lego::HttpServer http(jobs, *repo, catalog, cfg.webDist, opts);
    gServer = &http;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::cerr << "Lego Picture Generator listening on http://" << cfg.bind << ":" << cfg.port
              << " (hosts:";
    for (const auto& h : cfg.allowedHosts) {
      std::cerr << ' ' << h;
    }
    std::cerr << ", workers: " << cfg.workerCount
              << ", trustProxy: " << (cfg.trustProxy ? "yes" : "no")
              << ", uploads/min/ip: " << cfg.uploadsPerMinute << ")" << std::endl;
    if (!http.listen(cfg.bind, cfg.port)) {
      std::cerr << "Failed to bind " << cfg.bind << ":" << cfg.port << std::endl;
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}

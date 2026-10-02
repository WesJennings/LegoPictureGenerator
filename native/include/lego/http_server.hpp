#pragma once

#include "lego/jobs.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lego {

/** Edge/proxy behaviour. Defaults match the local-only setup. */
struct HttpOptions {
  std::vector<std::string> allowedHosts = {"localhost", "127.0.0.1"};
  bool trustProxy = false;
  int uploadsPerMinute = 0;
};

class HttpServer {
 public:
  HttpServer(JobService& jobs, FileJobRepository& repo, const Catalog& catalog,
             std::string webDist, HttpOptions options = {});
  ~HttpServer();

  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  bool bind(const std::string& host, int port);
  int bindAny(const std::string& host);
  bool listenAfterBind();
  bool listen(const std::string& host, int port);
  void stop();
  void waitUntilReady() const;
  int port() const { return port_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  int port_ = 0;
};

/** Host header without port must be in the allowlist (DNS-rebinding guard). */
bool isAllowedHostHeader(const std::string& hostHeader,
                         const std::vector<std::string>& allowedHosts);

/** Local-only convenience: allowlist of localhost / 127.0.0.1. */
bool isAllowedHostHeader(const std::string& hostHeader);

/**
 * Client IP for rate limiting. With trustProxy, prefer CF-Connecting-IP, then the
 * first X-Forwarded-For entry; otherwise the socket peer.
 */
std::string clientIpFor(const std::string& remoteAddr, const std::string& cfConnectingIp,
                        const std::string& xForwardedFor, bool trustProxy);

/** Fixed-window per-key limiter. Thread-safe. */
class RateLimiter {
 public:
  explicit RateLimiter(int perMinute) : perMinute_(perMinute) {}
  /** True if the request is allowed; records it. Always true when perMinute <= 0. */
  bool allow(const std::string& key, int64_t nowMs);

 private:
  struct Window {
    int64_t startMs = 0;
    int count = 0;
  };
  int perMinute_;
  std::mutex mu_;
  std::unordered_map<std::string, Window> windows_;
};

}  // namespace lego

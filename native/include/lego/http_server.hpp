#pragma once

#include "lego/jobs.hpp"

#include <memory>
#include <string>

namespace lego {

class HttpServer {
 public:
  HttpServer(JobService& jobs, FileJobRepository& repo, const Catalog& catalog,
             std::string webDist);
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

/** Host header without port must be localhost or 127.0.0.1 (DNS-rebinding guard). */
bool isAllowedHostHeader(const std::string& hostHeader);

}  // namespace lego

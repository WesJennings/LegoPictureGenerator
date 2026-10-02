#include "lego/http_server.hpp"

#include "lego/config.hpp"
#include "lego/pipeline.hpp"
#include "lego/types.hpp"
#include "lego/upload_validator.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "httplib.h"
#include "json.hpp"

namespace lego {
namespace {

using json = nlohmann::ordered_json;

struct HttpError : std::runtime_error {
  int status;
  HttpError(int s, const std::string& msg) : std::runtime_error(msg), status(s) {}
};

void setJson(httplib::Response& res, int status, const json& body) {
  res.status = status;
  res.set_content(body.dump(), "application/json");
}

void setError(httplib::Response& res, int status, const std::string& msg) {
  json j;
  j["error"] = msg;
  setJson(res, status, j);
}

std::string hostBare(const std::string& host) {
  // IPv6 literal: [::1]:8080 -> ::1
  if (!host.empty() && host[0] == '[') {
    auto close = host.find(']');
    return close == std::string::npos ? host : host.substr(1, close - 1);
  }
  auto colon = host.find(':');
  return colon == std::string::npos ? host : host.substr(0, colon);
}

std::string formField(const httplib::Request& req, const std::string& name) {
  if (req.has_file(name)) {
    return req.get_file_value(name).content;
  }
  if (req.has_param(name)) {
    return req.get_param_value(name);
  }
  return "";
}

std::string trimCopy(std::string s) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
  return s;
}

std::string toLowerCopy(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

std::string parseSizingMode(const std::string& raw) {
  std::string v = toLowerCopy(trimCopy(raw));
  if (v.empty()) {
    return SIZING_FIXED;
  }
  if (v == "fixed" || v == "auto" || v == "pieces") {
    return v;
  }
  throw HttpError(400, "sizing must be fixed, auto, or pieces");
}

int parseBoundedInt(const std::string& raw, int fallback, int lo, int hi,
                    const std::string& name) {
  std::string v = trimCopy(raw);
  if (v.empty()) {
    return fallback;
  }
  int value = 0;
  try {
    size_t idx = 0;
    value = std::stoi(v, &idx, 10);
    if (idx != v.size()) {
      throw std::invalid_argument("junk");
    }
  } catch (...) {
    throw HttpError(400, name + " must be a number");
  }
  if (value < lo || value > hi) {
    throw HttpError(400, name + " must be between " + std::to_string(lo) + " and " +
                             std::to_string(hi));
  }
  return value;
}

bool parseIncludeStudBom(const std::string& raw) {
  std::string v = toLowerCopy(trimCopy(raw));
  if (v.empty()) {
    return false;
  }
  return v == "true" || v == "1" || v == "on" || v == "yes";
}

std::vector<std::string> parseModesOr400(const std::string& raw) {
  try {
    return parsePackModes(raw, true);
  } catch (const std::invalid_argument& e) {
    std::string msg = e.what();
    if (msg.rfind("Unknown pack mode:", 0) == 0) {
      throw HttpError(400, msg);
    }
    throw HttpError(400, msg);
  }
}

bool safeArtifactName(const std::string& name) {
  if (name.empty() || name.size() > 128) {
    return false;
  }
  if (name.find("..") != std::string::npos) {
    return false;
  }
  for (unsigned char c : name) {
    if (!(std::isalnum(c) || c == '.' || c == '-' || c == '_')) {
      return false;
    }
  }
  return true;
}

bool pathInside(const std::filesystem::path& dir, const std::filesystem::path& file) {
  auto d = dir.lexically_normal();
  auto f = file.lexically_normal();
  auto ds = d.string();
  auto fs = f.string();
  if (ds.empty()) {
    return false;
  }
  if (ds.back() != '/' && ds.back() != '\\') {
    ds.push_back('/');
  }
  return fs.rfind(ds, 0) == 0;
}

void writeExact(const std::string& path, const std::string& bytes) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("could not write upload");
  }
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

}  // namespace

bool isAllowedHostHeader(const std::string& hostHeader,
                         const std::vector<std::string>& allowedHosts) {
  std::string bare = toLowerCopy(hostBare(trimCopy(hostHeader)));
  if (bare.empty()) {
    return false;
  }
  for (const auto& h : allowedHosts) {
    if (bare == h) {
      return true;
    }
  }
  return false;
}

bool isAllowedHostHeader(const std::string& hostHeader) {
  static const std::vector<std::string> kLocal = {"localhost", "127.0.0.1"};
  return isAllowedHostHeader(hostHeader, kLocal);
}

std::string clientIpFor(const std::string& remoteAddr, const std::string& cfConnectingIp,
                        const std::string& xForwardedFor, bool trustProxy) {
  if (trustProxy) {
    std::string cf = trimCopy(cfConnectingIp);
    if (!cf.empty()) {
      return cf;
    }
    std::string xff = xForwardedFor;
    auto comma = xff.find(',');
    if (comma != std::string::npos) {
      xff = xff.substr(0, comma);
    }
    xff = trimCopy(xff);
    if (!xff.empty()) {
      return xff;
    }
  }
  return remoteAddr;
}

bool RateLimiter::allow(const std::string& key, int64_t nowMs) {
  if (perMinute_ <= 0) {
    return true;
  }
  constexpr int64_t kWindowMs = 60'000;
  std::lock_guard<std::mutex> lock(mu_);
  // Bound memory: drop stale windows once the map gets large.
  if (windows_.size() > 10'000) {
    for (auto it = windows_.begin(); it != windows_.end();) {
      if (nowMs - it->second.startMs >= kWindowMs) {
        it = windows_.erase(it);
      } else {
        ++it;
      }
    }
  }
  Window& w = windows_[key];
  if (nowMs - w.startMs >= kWindowMs) {
    w.startMs = nowMs;
    w.count = 0;
  }
  if (w.count >= perMinute_) {
    return false;
  }
  w.count++;
  return true;
}

struct HttpServer::Impl {
  httplib::Server svr;
  HttpOptions options;
  RateLimiter limiter;
  explicit Impl(HttpOptions o) : options(std::move(o)), limiter(options.uploadsPerMinute) {}
};

HttpServer::HttpServer(JobService& jobs, FileJobRepository& repo, const Catalog& catalog,
                       std::string webDist, HttpOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {
  auto& svr = impl_->svr;
  const HttpOptions& opts = impl_->options;
  RateLimiter& limiter = impl_->limiter;
  svr.set_payload_max_length(static_cast<size_t>(MAX_UPLOAD_BYTES + 1024 * 1024));
  svr.set_pre_routing_handler([&opts](const httplib::Request& req, httplib::Response& res) {
    if (!isAllowedHostHeader(req.get_header_value("Host"), opts.allowedHosts)) {
      setError(res, 403, "Host not allowed");
      return httplib::Server::HandlerResponse::Handled;
    }
    return httplib::Server::HandlerResponse::Unhandled;
  });

  svr.Get("/api/v1/health", [&](const httplib::Request&, httplib::Response& res) {
    json j;
    j["status"] = "ok";
    j["paletteColors"] = static_cast<int>(catalog.palette.size());
    setJson(res, 200, j);
  });

  svr.Post("/api/v1/jobs", [&](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string ip = clientIpFor(req.remote_addr, req.get_header_value("CF-Connecting-IP"),
                                   req.get_header_value("X-Forwarded-For"), opts.trustProxy);
      if (!limiter.allow(ip, nowMs())) {
        res.set_header("Retry-After", "60");
        throw HttpError(429, "Too many uploads; try again in a minute");
      }
      if (!req.has_file("image")) {
        throw HttpError(400, "Multipart field 'image' is required");
      }
      const auto& upload = req.get_file_value("image");
      std::string sizingMode = parseSizingMode(formField(req, "sizing"));
      std::vector<std::string> modes = sizingMode == SIZING_PIECES
                                           ? std::vector<std::string>{"dlx"}
                                           : parseModesOr400(formField(req, "modes"));
      int targetStudCount = sizingMode == SIZING_AUTO
                                ? parseBoundedInt(formField(req, "targetStudCount"),
                                                  DEFAULT_TARGET_STUDS, MIN_TARGET_STUDS,
                                                  MAX_TARGET_STUDS, "targetStudCount")
                                : 0;
      int targetPieceCount = sizingMode == SIZING_PIECES
                                 ? parseBoundedInt(formField(req, "targetPieceCount"),
                                                   DEFAULT_TARGET_PIECES, MIN_TARGET_PIECES,
                                                   MAX_TARGET_PIECES, "targetPieceCount")
                                 : 0;
      bool includeStudBom = parseIncludeStudBom(formField(req, "includeStudBom"));

      JobManifest manifest =
          jobs.submit(modes, sizingMode, targetStudCount, targetPieceCount, includeStudBom);
      try {
        if (static_cast<int64_t>(upload.content.size()) > MAX_UPLOAD_BYTES) {
          throw InvalidUpload("Upload exceeds 25 MB limit");
        }
        writeExact(repo.inputFile(manifest.id), upload.content);
        validateUpload(repo.inputFile(manifest.id));
      } catch (...) {
        repo.deleteJob(manifest.id);
        throw;
      }
      jobs.enqueue(manifest, modes);
      json j;
      j["jobId"] = manifest.id;
      j["statusUrl"] = "/api/v1/jobs/" + manifest.id;
      setJson(res, 202, j);
    } catch (const HttpError& e) {
      setError(res, e.status, e.what());
    } catch (const InvalidUpload& e) {
      setError(res, 400, e.what());
    } catch (const QueueFullException& e) {
      setError(res, 429, e.what());
    } catch (const std::exception& e) {
      std::cerr << "Unhandled error: " << e.what() << std::endl;
      setError(res, 500, "Internal error");
    }
  });

  svr.Get(R"(/api/v1/jobs/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string jobId = req.matches[1];
      if (!isUuid(jobId)) {
        throw HttpError(404, "No such job");
      }
      auto m = jobs.findJob(jobId);
      if (!m) {
        throw HttpError(404, "No such job");
      }
      res.status = 200;
      res.set_content(jobApiJson(*m), "application/json");
    } catch (const HttpError& e) {
      setError(res, e.status, e.what());
    } catch (const std::exception& e) {
      std::cerr << "Unhandled error: " << e.what() << std::endl;
      setError(res, 500, "Internal error");
    }
  });

  svr.Get(R"(/api/v1/jobs/([^/]+)/artifacts/([^/]+))",
          [&](const httplib::Request& req, httplib::Response& res) {
            try {
              std::string jobId = req.matches[1];
              std::string name = req.matches[2];
              if (!isUuid(jobId)) {
                throw HttpError(404, "No such job");
              }
              auto m = jobs.findJob(jobId);
              if (!m) {
                throw HttpError(404, "No such job");
              }
              bool listed = false;
              for (const auto& [k, v] : m->artifacts) {
                if (v == name) {
                  listed = true;
                  break;
                }
              }
              if (!listed || !safeArtifactName(name)) {
                throw HttpError(404, "Unknown artifact");
              }
              auto dir = std::filesystem::path(repo.jobDir(m->id));
              auto file = (dir / name).lexically_normal();
              if (!pathInside(dir, file) || !std::filesystem::is_regular_file(file)) {
                throw HttpError(404, "Unknown artifact");
              }
              std::string contentType = "text/plain; charset=utf-8";
              if (name.size() >= 4 && name.substr(name.size() - 4) == ".png") {
                contentType = "image/png";
              } else if (name.size() >= 5 && name.substr(name.size() - 5) == ".json") {
                contentType = "application/json";
              }
              res.set_header("X-Content-Type-Options", "nosniff");
              if (contentType != "image/png") {
                res.set_header("Content-Disposition",
                               "attachment; filename=\"" + name + "\"");
              }
              res.set_file_content(file.string(), contentType);
            } catch (const HttpError& e) {
              setError(res, e.status, e.what());
            } catch (const std::exception& e) {
              std::cerr << "Unhandled error: " << e.what() << std::endl;
              setError(res, 500, "Internal error");
            }
          });

  svr.Delete(R"(/api/v1/jobs/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
    try {
      std::string jobId = req.matches[1];
      if (!isUuid(jobId)) {
        throw HttpError(404, "No such job");
      }
      auto m = jobs.findJob(jobId);
      if (!m) {
        throw HttpError(404, "No such job");
      }
      if (!jobs.deleteJob(m->id)) {
        throw HttpError(409, "Job is still queued or running");
      }
      res.status = 204;
      res.set_content("", "text/plain");
    } catch (const HttpError& e) {
      setError(res, e.status, e.what());
    } catch (const std::exception& e) {
      std::cerr << "Unhandled error: " << e.what() << std::endl;
      setError(res, 500, "Internal error");
    }
  });

  if (std::filesystem::is_directory(webDist)) {
    svr.set_mount_point("/", webDist);
  }

  svr.set_error_handler([webDist](const httplib::Request& req, httplib::Response& res) {
    if (res.status == 413) {
      setError(res, 400, "Upload exceeds 25 MB limit");
      return;
    }
    if (res.status == 404 && req.method == "GET" && req.path.rfind("/api/", 0) != 0) {
      auto index = std::filesystem::path(webDist) / "index.html";
      if (std::filesystem::is_regular_file(index)) {
        res.status = 200;
        res.set_file_content(index.string(), "text/html");
        return;
      }
    }
    if (res.body.empty()) {
      std::string msg = "Internal error";
      if (res.status == 404) {
        msg = "Not found";
      } else if (res.status == 400) {
        msg = "Bad request";
      }
      setError(res, res.status == 0 ? 500 : res.status, msg);
    }
  });

  svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
    try {
      if (ep) {
        std::rethrow_exception(ep);
      }
    } catch (const std::exception& e) {
      std::cerr << "Unhandled error: " << e.what() << std::endl;
    }
    setError(res, 500, "Internal error");
  });
}

HttpServer::~HttpServer() {
  if (impl_) {
    impl_->svr.stop();
  }
}

bool HttpServer::bind(const std::string& host, int port) {
  bool ok = impl_->svr.bind_to_port(host, port);
  if (ok) {
    port_ = port;
  }
  return ok;
}

int HttpServer::bindAny(const std::string& host) {
  int p = impl_->svr.bind_to_any_port(host);
  port_ = p;
  return p;
}

bool HttpServer::listenAfterBind() { return impl_->svr.listen_after_bind(); }

bool HttpServer::listen(const std::string& host, int port) {
  port_ = port;
  return impl_->svr.listen(host, port);
}

void HttpServer::stop() { impl_->svr.stop(); }

void HttpServer::waitUntilReady() const { impl_->svr.wait_until_ready(); }

}  // namespace lego

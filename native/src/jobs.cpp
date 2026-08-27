#include "lego/jobs.hpp"

#include "lego/image_io.hpp"
#include "lego/image_sampler.hpp"
#include "lego/piece_target.hpp"
#include "lego/text.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "json.hpp"

namespace lego {
namespace {

using json = nlohmann::ordered_json;

std::string newUuid() {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dist;
  uint64_t a = dist(gen);
  uint64_t b = dist(gen);
  uint8_t bytes[16];
  for (int i = 0; i < 8; i++) {
    bytes[i] = static_cast<uint8_t>((a >> ((7 - i) * 8)) & 0xff);
    bytes[8 + i] = static_cast<uint8_t>((b >> ((7 - i) * 8)) & 0xff);
  }
  bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0f) | 0x40);
  bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3f) | 0x80);
  static constexpr char hex[] = "0123456789abcdef";
  std::string out;
  out.resize(36);
  size_t p = 0;
  int bi = 0;
  auto hex2 = [&](int count) {
    for (int i = 0; i < count; i++) {
      out[p++] = hex[bytes[bi] >> 4];
      out[p++] = hex[bytes[bi] & 0x0f];
      bi++;
    }
  };
  hex2(4);
  out[p++] = '-';
  hex2(2);
  out[p++] = '-';
  hex2(2);
  out[p++] = '-';
  hex2(2);
  out[p++] = '-';
  hex2(6);
  return out;
}

json bomRowJson(const PipelineBomRow& row) {
  json o;
  o["partNum"] = row.partNum;
  o["w"] = row.w;
  o["h"] = row.h;
  o["colorId"] = row.colorId;
  o["colorName"] = row.colorName;
  o["count"] = row.count;
  return o;
}

json pipelineToJson(const PipelineResult& r) {
  json colorCounts = json::array();
  for (const auto& c : r.colorCounts) {
    json o;
    o["colorId"] = c.colorId;
    o["colorName"] = c.colorName;
    o["rgbHex"] = c.rgbHex;
    o["count"] = c.count;
    colorCounts.push_back(std::move(o));
  }
  json studBom = json::array();
  for (const auto& b : r.studBom) {
    studBom.push_back(bomRowJson(b));
  }
  json modeResults = json::array();
  for (const auto& m : r.modeResults) {
    json bom = json::array();
    for (const auto& b : m.bom) {
      bom.push_back(bomRowJson(b));
    }
    json o;
    o["mode"] = m.mode;
    o["status"] = m.status;
    o["elapsedMs"] = m.elapsedMs;
    o["pieceCount"] = m.pieceCount;
    o["bom"] = std::move(bom);
    modeResults.push_back(std::move(o));
  }
  json artifacts = json::object();
  for (const auto& [k, v] : r.artifacts) {
    artifacts[k] = v;
  }
  json out;
  out["gridWidth"] = r.gridWidth;
  out["gridHeight"] = r.gridHeight;
  out["totalStuds"] = r.totalStuds;
  out["colorCounts"] = std::move(colorCounts);
  out["studBom"] = std::move(studBom);
  out["modeResults"] = std::move(modeResults);
  out["artifacts"] = std::move(artifacts);
  return out;
}

PipelineBomRow bomRowFromJson(const json& o) {
  PipelineBomRow row;
  row.partNum = o.value("partNum", "");
  row.w = o.value("w", 0);
  row.h = o.value("h", 0);
  row.colorId = o.value("colorId", 0);
  row.colorName = o.value("colorName", "");
  row.count = o.value("count", 0);
  return row;
}

PipelineResult pipelineFromJson(const json& j) {
  PipelineResult r;
  r.gridWidth = j.value("gridWidth", 0);
  r.gridHeight = j.value("gridHeight", 0);
  r.totalStuds = j.value("totalStuds", 0);
  if (j.contains("colorCounts") && j["colorCounts"].is_array()) {
    for (const auto& c : j["colorCounts"]) {
      PipelineColorCountRow row;
      row.colorId = c.value("colorId", 0);
      row.colorName = c.value("colorName", "");
      row.rgbHex = c.value("rgbHex", "");
      row.count = c.value("count", 0);
      r.colorCounts.push_back(std::move(row));
    }
  }
  if (j.contains("studBom") && j["studBom"].is_array()) {
    for (const auto& b : j["studBom"]) {
      r.studBom.push_back(bomRowFromJson(b));
    }
  }
  if (j.contains("modeResults") && j["modeResults"].is_array()) {
    for (const auto& m : j["modeResults"]) {
      PipelineModeResult mr;
      mr.mode = m.value("mode", "");
      mr.status = m.value("status", "");
      mr.elapsedMs = m.value("elapsedMs", 0LL);
      mr.pieceCount = m.value("pieceCount", 0);
      if (m.contains("bom") && m["bom"].is_array()) {
        for (const auto& b : m["bom"]) {
          mr.bom.push_back(bomRowFromJson(b));
        }
      }
      r.modeResults.push_back(std::move(mr));
    }
  }
  if (j.contains("artifacts") && j["artifacts"].is_object()) {
    for (auto it = j["artifacts"].begin(); it != j["artifacts"].end(); ++it) {
      if (it.value().is_string()) {
        r.artifacts.emplace_back(it.key(), it.value().get<std::string>());
      }
    }
  }
  return r;
}

json artifactMapJson(const std::vector<std::pair<std::string, std::string>>& artifacts) {
  json o = json::object();
  for (const auto& [k, v] : artifacts) {
    o[k] = v;
  }
  return o;
}

json manifestToJson(const JobManifest& m) {
  json j;
  j["id"] = m.id;
  j["status"] = m.status;
  if (m.error) {
    j["error"] = *m.error;
  } else {
    j["error"] = nullptr;
  }
  j["createdAtEpochMs"] = m.createdAtEpochMs;
  j["updatedAtEpochMs"] = m.updatedAtEpochMs;
  j["blockSize"] = m.blockSize;
  j["targetStudWidth"] = m.targetStudWidth;
  j["sizingMode"] = m.sizingMode;
  j["autoSizing"] = m.autoSizing;
  j["targetStudCount"] = m.targetStudCount;
  j["targetPieceCount"] = m.targetPieceCount;
  j["pieceSearchAttempts"] = m.pieceSearchAttempts;
  j["achievedPieceCount"] = m.achievedPieceCount;
  j["includeStudBom"] = m.includeStudBom;
  j["renderStudSizePx"] = m.renderStudSizePx;
  j["modes"] = m.modes;
  j["artifacts"] = artifactMapJson(m.artifacts);
  if (m.result) {
    j["result"] = pipelineToJson(*m.result);
  } else {
    j["result"] = nullptr;
  }
  return j;
}

std::vector<std::pair<std::string, std::string>> artifactMapFromJson(const json& j) {
  std::vector<std::pair<std::string, std::string>> out;
  if (!j.is_object()) {
    return out;
  }
  for (auto it = j.begin(); it != j.end(); ++it) {
    if (it.value().is_string()) {
      out.emplace_back(it.key(), it.value().get<std::string>());
    }
  }
  return out;
}

JobManifest manifestFromJson(const json& j) {
  JobManifest m;
  m.id = j.value("id", "");
  m.status = j.value("status", "QUEUED");
  if (j.contains("error") && !j["error"].is_null()) {
    m.error = j["error"].get<std::string>();
  }
  m.createdAtEpochMs = j.value("createdAtEpochMs", 0LL);
  m.updatedAtEpochMs = j.value("updatedAtEpochMs", 0LL);
  m.blockSize = j.value("blockSize", 0);
  m.targetStudWidth = j.value("targetStudWidth", 0);
  m.sizingMode = j.value("sizingMode", std::string(SIZING_FIXED));
  m.autoSizing = j.value("autoSizing", false);
  m.targetStudCount = j.value("targetStudCount", 0);
  m.targetPieceCount = j.value("targetPieceCount", 0);
  m.pieceSearchAttempts = j.value("pieceSearchAttempts", 0);
  m.achievedPieceCount = j.value("achievedPieceCount", 0);
  m.includeStudBom = j.value("includeStudBom", false);
  m.renderStudSizePx = j.value("renderStudSizePx", 0);
  if (j.contains("modes") && j["modes"].is_array()) {
    m.modes = j["modes"].get<std::vector<std::string>>();
  }
  if (j.contains("artifacts")) {
    m.artifacts = artifactMapFromJson(j["artifacts"]);
  }
  if (j.contains("result") && !j["result"].is_null()) {
    m.result = pipelineFromJson(j["result"]);
  }
  return m;
}

int64_t directoryBytes(const std::string& dir) {
  int64_t total = 0;
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) {
    return 0;
  }
  for (auto it = std::filesystem::recursive_directory_iterator(
           dir, std::filesystem::directory_options::skip_permission_denied, ec);
       it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) {
      break;
    }
    std::error_code fec;
    if (it->is_regular_file(fec)) {
      auto sz = it->file_size(fec);
      if (!fec) {
        total += static_cast<int64_t>(sz);
      }
    }
  }
  return total;
}

std::string userSafeMessage(const std::exception& ex) {
  std::string msg = ex.what();
  if (msg.empty()) {
    return "Processing failed";
  }
  return msg;
}

}  // namespace

bool isUuid(const std::string& s) {
  if (s.size() != 36) {
    return false;
  }
  for (size_t i = 0; i < s.size(); i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (s[i] != '-') {
        return false;
      }
    } else {
      char c = s[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
        return false;
      }
    }
  }
  return true;
}

FileJobRepository::FileJobRepository(std::string root) : root_(std::move(root)) {
  std::filesystem::create_directories(root_);
}

std::string FileJobRepository::jobDir(const std::string& jobId) const {
  return root_ + "/" + jobId;
}

std::string FileJobRepository::inputFile(const std::string& jobId) const {
  return jobDir(jobId) + "/input.png";
}

void FileJobRepository::writeManifestUnlocked(const JobManifest& m) {
  auto dir = std::filesystem::path(jobDir(m.id));
  std::filesystem::create_directories(dir);
  auto tmp = dir / "manifest.json.tmp";
  auto dst = dir / "manifest.json";
  {
    std::ofstream out(tmp);
    if (!out) {
      throw std::runtime_error("could not write manifest");
    }
    out << manifestToJson(m).dump(2);
  }
  std::filesystem::rename(tmp, dst);
}

std::optional<JobManifest> FileJobRepository::readManifestUnlocked(const std::string& jobId) {
  auto path = std::filesystem::path(jobDir(jobId)) / "manifest.json";
  if (!std::filesystem::is_regular_file(path)) {
    return std::nullopt;
  }
  try {
    std::ifstream in(path);
    json j;
    in >> j;
    JobManifest m = manifestFromJson(j);
    if (m.id.empty()) {
      m.id = jobId;
    }
    return m;
  } catch (...) {
    return std::nullopt;
  }
}

JobManifest FileJobRepository::createJob(int blockSize, const std::string& sizingMode,
                                         int targetStudCount, int targetPieceCount,
                                         bool includeStudBom, int renderStudSizePx,
                                         const std::vector<std::string>& modes) {
  enforceRetention();
  JobManifest m;
  m.id = newUuid();
  m.createdAtEpochMs = nowMs();
  m.updatedAtEpochMs = m.createdAtEpochMs;
  m.blockSize = blockSize;
  m.sizingMode = sizingMode.empty() ? SIZING_FIXED : sizingMode;
  m.autoSizing = m.sizingMode == SIZING_AUTO;
  m.targetStudWidth = m.sizingMode == SIZING_FIXED ? CLASSIC_STUD_WIDTH : 0;
  m.targetStudCount = targetStudCount;
  m.targetPieceCount = targetPieceCount;
  m.includeStudBom = includeStudBom;
  m.renderStudSizePx = renderStudSizePx;
  m.modes = modes;
  m.status = "QUEUED";
  std::filesystem::create_directories(jobDir(m.id));
  std::lock_guard<std::mutex> lock(mu_);
  writeManifestUnlocked(m);
  return m;
}

void FileJobRepository::saveManifest(JobManifest& m) {
  m.updatedAtEpochMs = nowMs();
  std::lock_guard<std::mutex> lock(mu_);
  writeManifestUnlocked(m);
}

std::optional<JobManifest> FileJobRepository::findJob(const std::string& jobId) {
  if (!isUuid(jobId)) {
    return std::nullopt;
  }
  std::lock_guard<std::mutex> lock(mu_);
  return readManifestUnlocked(jobId);
}

std::vector<JobManifest> FileJobRepository::listJobs() {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<JobManifest> out;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(root_, ec)) {
    if (!entry.is_directory()) {
      continue;
    }
    auto name = entry.path().filename().string();
    auto loaded = readManifestUnlocked(name);
    if (loaded) {
      out.push_back(std::move(*loaded));
    }
  }
  std::sort(out.begin(), out.end(), [](const JobManifest& a, const JobManifest& b) {
    return a.createdAtEpochMs > b.createdAtEpochMs;
  });
  return out;
}

void FileJobRepository::deleteJob(const std::string& jobId) {
  std::lock_guard<std::mutex> lock(mu_);
  std::error_code ec;
  std::filesystem::remove_all(jobDir(jobId), ec);
}

void FileJobRepository::recoverInterruptedJobs() {
  for (auto m : listJobs()) {
    if (!isTerminalStatus(m.status)) {
      m.status = "FAILED";
      m.error = "Interrupted by backend restart";
      try {
        saveManifest(m);
      } catch (...) {
      }
    }
  }
}

void FileJobRepository::enforceRetention() {
  auto jobs = listJobs();
  int64_t cutoff = nowMs() - MAX_JOB_AGE_MS;
  for (const auto& m : jobs) {
    if (m.createdAtEpochMs < cutoff && isTerminalStatus(m.status)) {
      deleteJob(m.id);
    }
  }
  auto remaining = listJobs();
  std::sort(remaining.begin(), remaining.end(), [](const JobManifest& a, const JobManifest& b) {
    return a.createdAtEpochMs < b.createdAtEpochMs;
  });
  int count = static_cast<int>(remaining.size());
  int64_t totalBytes = directoryBytes(root_);
  for (const auto& m : remaining) {
    if (count < MAX_JOBS && totalBytes < MAX_TOTAL_BYTES) {
      break;
    }
    if (!isTerminalStatus(m.status)) {
      continue;
    }
    int64_t freed = directoryBytes(jobDir(m.id));
    deleteJob(m.id);
    count--;
    totalBytes -= freed;
  }
}

int64_t timeoutForMs(int modeCount, bool pieceSearch) {
  if (pieceSearch) {
    return MAX_JOB_TIMEOUT_MS;
  }
  if (modeCount <= 1) {
    return JOB_TIMEOUT_MS;
  }
  int64_t scaled = PER_MODE_BUDGET_MS * static_cast<int64_t>(modeCount);
  return scaled > MAX_JOB_TIMEOUT_MS ? MAX_JOB_TIMEOUT_MS : scaled;
}

JobService::JobService(std::shared_ptr<FileJobRepository> repo, Catalog catalog, int workerCount)
    : repo_(std::move(repo)), catalog_(std::move(catalog)), workerCount_(std::max(1, workerCount)) {
  for (int i = 0; i < workerCount_; i++) {
    workers_.emplace_back([this] { workerLoop(); });
  }
}

JobService::~JobService() { shutdown(); }

void JobService::shutdown() {
  stop_ = true;
  cv_.notify_all();
  for (auto& t : workers_) {
    if (t.joinable()) {
      t.join();
    }
  }
  workers_.clear();
}

JobManifest JobService::submit(const std::vector<std::string>& modes, const std::string& sizingMode,
                               int targetStudCount, int targetPieceCount, bool includeStudBom) {
  return repo_->createJob(DEFAULT_BLOCK_SIZE, sizingMode,
                          sizingMode == SIZING_AUTO ? targetStudCount : 0,
                          sizingMode == SIZING_PIECES ? targetPieceCount : 0, includeStudBom,
                          DEFAULT_RENDER_STUD_PX, modes);
}

void JobService::enqueue(const JobManifest& manifest, const std::vector<std::string>& modes) {
  std::unique_lock<std::mutex> lock(mu_);
  if (static_cast<int>(queue_.size()) >= QUEUE_CAPACITY) {
    lock.unlock();
    try {
      repo_->deleteJob(manifest.id);
    } catch (...) {
    }
    throw QueueFullException();
  }
  queue_.push_back(Task{manifest.id, modes});
  lock.unlock();
  cv_.notify_one();
}

std::optional<JobManifest> JobService::findJob(const std::string& jobId) {
  return repo_->findJob(jobId);
}

bool JobService::deleteJob(const std::string& jobId) {
  auto m = repo_->findJob(jobId);
  if (!m) {
    return true;
  }
  if (!isTerminalStatus(m->status)) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = cancels_.find(jobId);
    if (it != cancels_.end() && it->second) {
      it->second->store(true);
    }
  }
  repo_->deleteJob(jobId);
  return true;
}

void JobService::workerLoop() {
  while (!stop_) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [&] { return stop_ || !queue_.empty(); });
      if (stop_ && queue_.empty()) {
        return;
      }
      if (queue_.empty()) {
        continue;
      }
      task = std::move(queue_.front());
      queue_.erase(queue_.begin());
      running_++;
    }
    try {
      execute(task.jobId, task.modes);
    } catch (...) {
    }
    {
      std::lock_guard<std::mutex> lock(mu_);
      running_--;
    }
  }
}

void JobService::updateStatus(JobManifest& manifest, const std::string& status,
                              const std::optional<std::string>& error) {
  manifest.status = status;
  manifest.error = error;
  try {
    repo_->saveManifest(manifest);
  } catch (...) {
  }
}

void JobService::resolveSizing(JobManifest& manifest, const std::atomic<bool>* cancel) {
  if (manifest.isPieceSizing()) {
    updateStatus(manifest, "SEARCHING", std::nullopt);
    DecodedImage src = loadImageArgb(repo_->inputFile(manifest.id));
    int target = manifest.targetPieceCount > 0 ? manifest.targetPieceCount : DEFAULT_TARGET_PIECES;
    PieceSearchResult search = solvePieceTarget(
        src.width, src.height, target, [&](int blockSize) {
          return probePieceCount(src, catalog_, blockSize, "dlx", cancel);
        });
    manifest.blockSize = search.blockSize;
    manifest.targetStudCount = search.studTargetUsed;
    manifest.pieceSearchAttempts = search.attempts;
    manifest.achievedPieceCount = search.bestPieceCount;
    manifest.modes = {"dlx"};
    repo_->saveManifest(manifest);
    return;
  }
  if (!manifest.isAutoStudSizing() || manifest.targetStudCount < 1) {
    return;
  }
  DecodedImage src = loadImageArgb(repo_->inputFile(manifest.id));
  manifest.blockSize = blockSizeForTargetStuds(src.width, src.height, manifest.targetStudCount);
  repo_->saveManifest(manifest);
}

void JobService::execute(const std::string& jobId, const std::vector<std::string>& modes) {
  auto loaded = repo_->findJob(jobId);
  if (!loaded) {
    return;
  }
  JobManifest manifest = *loaded;
  std::atomic<bool> cancel{false};
  {
    std::lock_guard<std::mutex> lock(mu_);
    cancels_[jobId] = &cancel;
  }

  try {
    resolveSizing(manifest, &cancel);
    loaded = repo_->findJob(jobId);
    if (loaded) {
      manifest = *loaded;
    }

    std::vector<std::string> runModes =
        manifest.isPieceSizing() ? std::vector<std::string>{"dlx"} : orderedPackModes(modes);
    int studWidth = manifest.targetStudWidth > 0
                        ? manifest.targetStudWidth
                        : (manifest.effectiveSizingMode() == SIZING_FIXED ? CLASSIC_STUD_WIDTH : 0);
    JobConfig cfg;
    cfg.inputPath = repo_->inputFile(jobId);
    cfg.outputDirectory = repo_->jobDir(jobId);
    cfg.blockSize = manifest.blockSize;
    cfg.targetStudWidth = studWidth;
    cfg.renderStudSizePx = manifest.renderStudSizePx;
    cfg.modes = runModes;
    cfg.includeStudBom = manifest.includeStudBom;

    int64_t timeoutMs = timeoutForMs(static_cast<int>(runModes.size()), manifest.isPieceSizing());

    std::optional<PipelineResult> result;
    std::exception_ptr ep;
    std::atomic<bool> done{false};
    std::thread runner([&] {
      try {
        result = runPipeline(
            catalog_, cfg,
            [&](const std::string& stage) { updateStatus(manifest, stage, std::nullopt); },
            &cancel);
      } catch (...) {
        ep = std::current_exception();
      }
      done = true;
    });

    auto start = std::chrono::steady_clock::now();
    while (!done.load()) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
      if (elapsed >= timeoutMs) {
        cancel = true;
        runner.join();
        updateStatus(manifest, "FAILED",
                     "Timed out after " + std::to_string(timeoutMs / 1000) + "s");
        std::lock_guard<std::mutex> lock(mu_);
        cancels_.erase(jobId);
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    runner.join();
    if (ep) {
      try {
        std::rethrow_exception(ep);
      } catch (const std::exception& ex) {
        updateStatus(manifest, "FAILED", userSafeMessage(ex));
      }
    } else if (result) {
      manifest.result = *result;
      manifest.artifacts.insert(manifest.artifacts.end(), result->artifacts.begin(),
                                result->artifacts.end());
      if (manifest.isPieceSizing() && result->modeResults.size() > 0) {
        manifest.achievedPieceCount = result->modeResults[0].pieceCount;
      }
      updateStatus(manifest, "COMPLETE", std::nullopt);
    } else {
      updateStatus(manifest, "FAILED", "Processing failed");
    }
  } catch (const std::exception& ex) {
    updateStatus(manifest, "FAILED", userSafeMessage(ex));
  }

  std::lock_guard<std::mutex> lock(mu_);
  cancels_.erase(jobId);
}

std::string jobApiJson(const JobManifest& m) {
  json settings;
  settings["blockSize"] = m.blockSize;
  settings["targetStudWidth"] = m.targetStudWidth;
  settings["sizingMode"] = m.effectiveSizingMode();
  settings["autoSizing"] = m.isAutoStudSizing();
  settings["targetStudCount"] = m.targetStudCount;
  settings["targetPieceCount"] = m.targetPieceCount;
  settings["pieceSearchAttempts"] = m.pieceSearchAttempts;
  settings["achievedPieceCount"] = m.achievedPieceCount;
  settings["includeStudBom"] = m.includeStudBom;
  settings["renderStudSizePx"] = m.renderStudSizePx;
  settings["modes"] = m.modes;

  json artifactUrls = json::object();
  for (const auto& [k, v] : m.artifacts) {
    artifactUrls[k] = "/api/v1/jobs/" + m.id + "/artifacts/" + v;
  }

  json out;
  out["jobId"] = m.id;
  out["status"] = m.status;
  if (m.error) {
    out["error"] = *m.error;
  } else {
    out["error"] = nullptr;
  }
  out["createdAtEpochMs"] = m.createdAtEpochMs;
  out["updatedAtEpochMs"] = m.updatedAtEpochMs;
  out["settings"] = std::move(settings);
  if (m.result) {
    out["result"] = pipelineToJson(*m.result);
  } else {
    out["result"] = nullptr;
  }
  out["artifacts"] = std::move(artifactUrls);
  return out.dump();
}

}  // namespace lego

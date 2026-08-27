#pragma once

#include "lego/catalog.hpp"
#include "lego/pipeline.hpp"
#include "lego/types.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lego {

struct JobManifest {
  std::string id;
  std::string status = "QUEUED";
  std::optional<std::string> error;
  int64_t createdAtEpochMs = 0;
  int64_t updatedAtEpochMs = 0;
  int blockSize = 0;
  int targetStudWidth = 0;
  std::string sizingMode = SIZING_FIXED;
  bool autoSizing = false;
  int targetStudCount = 0;
  int targetPieceCount = 0;
  int pieceSearchAttempts = 0;
  int achievedPieceCount = 0;
  bool includeStudBom = false;
  int renderStudSizePx = 0;
  std::vector<std::string> modes;
  std::vector<std::pair<std::string, std::string>> artifacts;
  std::optional<PipelineResult> result;

  std::string effectiveSizingMode() const {
    if (!sizingMode.empty()) {
      return sizingMode;
    }
    return autoSizing ? SIZING_AUTO : SIZING_FIXED;
  }

  bool isPieceSizing() const { return effectiveSizingMode() == SIZING_PIECES; }

  bool isAutoStudSizing() const {
    return effectiveSizingMode() == SIZING_AUTO || autoSizing;
  }
};

class QueueFullException : public std::runtime_error {
 public:
  QueueFullException() : std::runtime_error("Job queue is full; try again shortly") {}
};

bool isUuid(const std::string& s);

class FileJobRepository {
 public:
  explicit FileJobRepository(std::string root);
  const std::string& root() const { return root_; }

  JobManifest createJob(int blockSize, const std::string& sizingMode, int targetStudCount,
                        int targetPieceCount, bool includeStudBom, int renderStudSizePx,
                        const std::vector<std::string>& modes);

  std::string jobDir(const std::string& jobId) const;
  std::string inputFile(const std::string& jobId) const;

  void saveManifest(JobManifest& m);
  std::optional<JobManifest> findJob(const std::string& jobId);
  std::vector<JobManifest> listJobs();
  void deleteJob(const std::string& jobId);
  void recoverInterruptedJobs();
  void enforceRetention();

 private:
  std::string root_;
  std::mutex mu_;
  void writeManifestUnlocked(const JobManifest& m);
  std::optional<JobManifest> readManifestUnlocked(const std::string& jobId);
};

int64_t timeoutForMs(int modeCount, bool pieceSearch);

class JobService {
 public:
  JobService(std::shared_ptr<FileJobRepository> repo, Catalog catalog, int workerCount);
  ~JobService();

  JobService(const JobService&) = delete;
  JobService& operator=(const JobService&) = delete;

  JobManifest submit(const std::vector<std::string>& modes, const std::string& sizingMode,
                     int targetStudCount, int targetPieceCount, bool includeStudBom);
  void enqueue(const JobManifest& manifest, const std::vector<std::string>& modes);
  std::optional<JobManifest> findJob(const std::string& jobId);
  /** Deletes a terminal job. Returns false if still queued or running. Missing → true. */
  bool deleteJob(const std::string& jobId);
  void shutdown();

 private:
  struct Task {
    std::string jobId;
    std::vector<std::string> modes;
  };

  std::shared_ptr<FileJobRepository> repo_;
  Catalog catalog_;
  int workerCount_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::vector<Task> queue_;
  int running_ = 0;
  std::atomic<bool> stop_{false};
  std::vector<std::thread> workers_;
  std::unordered_map<std::string, std::atomic<bool>*> cancels_;

  void workerLoop();
  void execute(const std::string& jobId, const std::vector<std::string>& modes);
  void updateStatus(JobManifest& manifest, const std::string& status,
                    const std::optional<std::string>& error);
  void resolveSizing(JobManifest& manifest, const std::atomic<bool>* cancel);
};

/** GET /api/v1/jobs/{id} body (Jackson-shaped). */
std::string jobApiJson(const JobManifest& m);

}  // namespace lego

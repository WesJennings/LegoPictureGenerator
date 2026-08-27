#include "lego/catalog.hpp"
#include "lego/http_server.hpp"
#include "lego/image_io.hpp"
#include "lego/image_sampler.hpp"
#include "lego/jobs.hpp"
#include "lego/packers.hpp"
#include "lego/piece_target.hpp"
#include "lego/pipeline.hpp"
#include "lego/renderer.hpp"
#include "lego/types.hpp"
#include "lego/upload_validator.hpp"

#include "httplib.h"
#include "json.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace lego;
using json = nlohmann::json;

namespace {

struct TempDir {
  std::filesystem::path path;
  TempDir() {
    auto base = std::filesystem::temp_directory_path() / "lego-host-tests";
    std::filesystem::create_directories(base);
    path = base / ("t-" + std::to_string(nowMs()) + "-" + std::to_string(rand()));
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

void sqlExec(sqlite3* db, const char* sql) {
  char* err = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "sqlite error";
    sqlite3_free(err);
    throw std::runtime_error(msg);
  }
}

std::string makeFixtureDb(const std::filesystem::path& dir, bool include3022) {
  auto path = dir / "bricks.db";
  sqlite3* db = nullptr;
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    throw std::runtime_error("could not create fixture db");
  }
  struct Closer {
    sqlite3* db;
    ~Closer() { sqlite3_close(db); }
  } closer{db};
  sqlExec(db, "CREATE TABLE colors (id INTEGER, name TEXT, rgb TEXT, is_trans TEXT)");
  sqlExec(db, "CREATE TABLE elements (part_num TEXT, color_id INTEGER)");
  sqlExec(db, "INSERT INTO colors VALUES (0, 'Black', '05131D', 'f')");
  sqlExec(db, "INSERT INTO colors VALUES (15, 'White', 'FFFFFF', 'f')");
  sqlExec(db, "INSERT INTO elements VALUES ('3024', 0)");
  sqlExec(db, "INSERT INTO elements VALUES ('3024', 15)");
  sqlExec(db, "INSERT INTO elements VALUES ('3023', 0)");
  sqlExec(db, "INSERT INTO elements VALUES ('3023', 15)");
  if (include3022) {
    sqlExec(db, "INSERT INTO elements VALUES ('3022', 0)");
    sqlExec(db, "INSERT INTO elements VALUES ('3022', 15)");
  }
  return path.string();
}

void writeSolidPng(const std::string& path, int w, int h, int argb) {
  std::vector<int> px(static_cast<size_t>(w) * h, argb);
  writePngArgb(path, px.data(), w, h);
}

void writeCheckerPng(const std::string& path, int size) {
  std::vector<int> px(static_cast<size_t>(size) * size);
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      px[y * size + x] = x < size / 2 ? 0xFF000000 : 0xFFFFFFFF;
    }
  }
  writePngArgb(path, px.data(), size, size);
}

std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::vector<unsigned char> pngHeader(int w, int h) {
  std::vector<unsigned char> b(24, 0);
  static const unsigned char sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  std::copy(sig, sig + 8, b.begin());
  b[12] = 'I';
  b[13] = 'H';
  b[14] = 'D';
  b[15] = 'R';
  auto be32 = [&](int off, uint32_t v) {
    b[off] = static_cast<unsigned char>((v >> 24) & 0xff);
    b[off + 1] = static_cast<unsigned char>((v >> 16) & 0xff);
    b[off + 2] = static_cast<unsigned char>((v >> 8) & 0xff);
    b[off + 3] = static_cast<unsigned char>(v & 0xff);
  };
  be32(16, static_cast<uint32_t>(w));
  be32(20, static_cast<uint32_t>(h));
  return b;
}

}  // namespace

static void testUploadValidator() {
  TempDir tmp;
  auto small = (tmp.path / "img.png").string();
  writeSolidPng(small, 50, 50, 0xFF112233);
  validateUpload(small);

  auto fake = tmp.path / "fake.png";
  {
    std::ofstream out(fake);
    out << "definitely not an image";
  }
  bool threw = false;
  try {
    validateUpload(fake.string());
  } catch (const InvalidUpload&) {
    threw = true;
  }
  assert(threw);

  auto big = tmp.path / "big.png";
  {
    auto bytes = pngHeader(6000, 6000);
    std::ofstream out(big, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }
  threw = false;
  try {
    validateUpload(big.string());
  } catch (const InvalidUpload& e) {
    threw = true;
    std::string msg = e.what();
    assert(msg.find("too large") != std::string::npos);
  }
  assert(threw);

  auto stub = tmp.path / "stub.png";
  {
    std::ofstream out(stub, std::ios::binary);
    unsigned char b = 0x89;
    out.write(reinterpret_cast<const char*>(&b), 1);
  }
  threw = false;
  try {
    validateUpload(stub.string());
  } catch (const InvalidUpload&) {
    threw = true;
  }
  assert(threw);
}

static void testPieceTarget() {
  int srcW = 4000;
  int srcH = 3000;
  int targetPieces = 800;
  double ratio = 0.28;
  auto result = solvePieceTarget(srcW, srcH, targetPieces, [&](int blockSize) {
    int studs = studCountFor(srcW, srcH, blockSize);
    return std::max(1, static_cast<int>(std::lround(studs * ratio)));
  });
  assert(result.attempts >= 1);
  assert(result.attempts <= MAX_PIECE_PROBES);
  double rel = std::abs(result.bestPieceCount - targetPieces) / static_cast<double>(targetPieces);
  assert(rel <= PIECE_SEARCH_TOLERANCE + 0.02);
  assert(result.blockSize >= MIN_BLOCK_SIZE);
  assert(result.blockSize <= MAX_BLOCK_SIZE);

  auto stuck = solvePieceTarget(800, 600, 500, [](int) { return 2000; });
  assert(stuck.bestPieceCount == 2000);
  assert(stuck.attempts >= 1);
}

static void testTimeoutFor() {
  assert(timeoutForMs(1, true) == MAX_JOB_TIMEOUT_MS);
  assert(timeoutForMs(1, false) == JOB_TIMEOUT_MS);
  assert(timeoutForMs(2, false) == PER_MODE_BUDGET_MS * 2);
  assert(timeoutForMs(20, false) == MAX_JOB_TIMEOUT_MS);
}

static void testFileJobRepoSizing() {
  TempDir tmp;
  FileJobRepository repo(tmp.path.string());
  auto m = repo.createJob(DEFAULT_BLOCK_SIZE, SIZING_PIECES, 0, 800, false,
                          DEFAULT_RENDER_STUD_PX, {"dlx"});
  assert(m.sizingMode == SIZING_PIECES);
  assert(m.targetPieceCount == 800);
  assert(!m.autoSizing);
  assert(m.isPieceSizing());
  auto loaded = repo.findJob(m.id);
  assert(loaded);
  assert(loaded->effectiveSizingMode() == SIZING_PIECES);
  assert(loaded->targetPieceCount == 800);
  assert(loaded->modes.size() == 1 && loaded->modes[0] == "dlx");
}

static void testRecoverInterrupted() {
  TempDir tmp;
  FileJobRepository repo(tmp.path.string());
  auto m = repo.createJob(DEFAULT_BLOCK_SIZE, SIZING_FIXED, 0, 0, false, DEFAULT_RENDER_STUD_PX,
                          {"greedy"});
  assert(m.status == "QUEUED");
  repo.recoverInterruptedJobs();
  auto loaded = repo.findJob(m.id);
  assert(loaded);
  assert(loaded->status == "FAILED");
  assert(loaded->error && *loaded->error == "Interrupted by backend restart");
}

static void testPipelineGreedy() {
  TempDir tmp;
  Catalog cat = loadCatalog(makeFixtureDb(tmp.path, false));
  auto input = (tmp.path / "input.png").string();
  writeCheckerPng(input, 160);
  auto out = (tmp.path / "job-out").string();
  JobConfig cfg;
  cfg.inputPath = input;
  cfg.outputDirectory = out;
  cfg.blockSize = 8;
  cfg.targetStudWidth = 0;
  cfg.renderStudSizePx = 8;
  cfg.modes = {"greedy"};
  cfg.includeStudBom = false;
  PipelineResult result = runPipeline(cat, cfg);
  assert(result.gridWidth == 20);
  assert(result.gridHeight == 20);
  assert(result.totalStuds == 400);
  assert(result.modeResults.size() == 1);
  const auto& greedy = result.modeResults[0];
  int bomTotal = 0;
  int studsCovered = 0;
  for (const auto& row : greedy.bom) {
    bomTotal += row.count;
    studsCovered += row.count * row.w * row.h;
  }
  assert(bomTotal == greedy.pieceCount);
  assert(studsCovered == 400);
  for (const char* name : {"matched.png", "lego-studs.png", "lego-greedy.png", "bom-greedy.txt",
                           "placements-greedy.json", "color-counts.txt"}) {
    assert(std::filesystem::is_regular_file(std::filesystem::path(out) / name));
  }
  assert(result.studBom.empty());
}

static void testPipelineClassicWidth() {
  TempDir tmp;
  Catalog cat = loadCatalog(makeFixtureDb(tmp.path, false));
  auto input = (tmp.path / "classic-input.png").string();
  std::vector<int> px(320 * 480);
  for (int y = 0; y < 480; y++) {
    for (int x = 0; x < 320; x++) {
      px[y * 320 + x] = x < 160 ? 0xFF000000 : 0xFFFFFFFF;
    }
  }
  writePngArgb(input, px.data(), 320, 480);
  auto out = (tmp.path / "job-classic").string();
  JobConfig cfg;
  cfg.inputPath = input;
  cfg.outputDirectory = out;
  cfg.blockSize = DEFAULT_BLOCK_SIZE;
  cfg.targetStudWidth = CLASSIC_STUD_WIDTH;
  cfg.renderStudSizePx = 8;
  cfg.modes = {"greedy"};
  PipelineResult result = runPipeline(cat, cfg);
  assert(result.gridWidth == 54);
  assert(result.gridHeight == 81);
  assert(result.totalStuds > 100);
}

static void testPipelineStudBom() {
  TempDir tmp;
  Catalog cat = loadCatalog(makeFixtureDb(tmp.path, false));
  auto input = (tmp.path / "input.png").string();
  writeCheckerPng(input, 160);
  auto out = (tmp.path / "job-studs").string();
  JobConfig cfg;
  cfg.inputPath = input;
  cfg.outputDirectory = out;
  cfg.blockSize = 8;
  cfg.renderStudSizePx = 8;
  cfg.modes = {"greedy"};
  cfg.includeStudBom = true;
  PipelineResult result = runPipeline(cat, cfg);
  assert(std::filesystem::is_regular_file(std::filesystem::path(out) / "bom-studs.txt"));
  assert(result.studBom.size() == 2);
  int studTotal = 0;
  for (const auto& row : result.studBom) {
    studTotal += row.count;
    assert(row.w == 1 && row.h == 1);
  }
  assert(studTotal == 400);
}

static void testCatalogParityAndRender() {
  TempDir tmp;
  Catalog cat = loadCatalog(makeFixtureDb(tmp.path, true));
  assert(cat.palette.size() == 2);
  assert(nearestIndex(0xFF000000, paletteRgb(cat.palette)) == 0);
  assert(nearestIndex(0xFFFFFFFF, paletteRgb(cat.palette)) == 1);

  StudGrid studs(3, std::vector<LegoElement>(4));
  for (int y = 0; y < 3; y++) {
    for (int x = 0; x < 4; x++) {
      studs[y][x] = x < 2 ? cat.palette[0] : cat.palette[1];
    }
  }
  std::vector<int> matched(12);
  for (int y = 0; y < 3; y++) {
    for (int x = 0; x < 4; x++) {
      matched[y * 4 + x] = studs[y][x].toArgb();
    }
  }
  auto studsImg = renderStuds(matched.data(), 4, 3, 8);
  assert(static_cast<int>(studsImg.size()) == 32 * 24);
  auto packed = packGreedy(cat.plates, studs);
  auto packedImg = renderPacked(matched.data(), 4, 3, 8, packed.placed);
  assert(static_cast<int>(packedImg.size()) == 32 * 24);
  assert((static_cast<uint32_t>(packedImg[0]) >> 24) == 0xFFu);
}

static void testHostHeaderGuard() {
  assert(isAllowedHostHeader("localhost"));
  assert(isAllowedHostHeader("localhost:8080"));
  assert(isAllowedHostHeader("127.0.0.1"));
  assert(isAllowedHostHeader("127.0.0.1:8080"));
  assert(!isAllowedHostHeader("evil.example"));
  assert(!isAllowedHostHeader("example.com:8080"));
  assert(!isAllowedHostHeader(""));
}

static void testHttpApi() {
  TempDir tmp;
  Catalog cat = loadCatalog(makeFixtureDb(tmp.path, false));
  auto repo = std::make_shared<FileJobRepository>((tmp.path / "jobs").string());
  JobService jobs(repo, cat, 1);
  HttpServer http(jobs, *repo, cat, "");
  int port = http.bindAny("127.0.0.1");
  assert(port > 0);
  std::thread serverThread([&] { http.listenAfterBind(); });
  http.waitUntilReady();

  httplib::Client cli("127.0.0.1", port);
  cli.set_connection_timeout(2, 0);
  cli.set_read_timeout(60, 0);

  auto health = cli.Get("/api/v1/health");
  assert(health);
  assert(health->status == 200);
  auto hj = json::parse(health->body);
  assert(hj["status"] == "ok");
  assert(hj["paletteColors"] == 2);

  auto missing = cli.Get("/api/v1/jobs/not-a-uuid");
  assert(missing);
  assert(missing->status == 404);
  auto mj = json::parse(missing->body);
  assert(mj["error"] == "No such job");

  auto unknown = cli.Get("/api/v1/jobs/00000000-0000-4000-8000-000000000000");
  assert(unknown);
  assert(unknown->status == 404);

  auto noImage = cli.Post("/api/v1/jobs", httplib::MultipartFormDataItems{
                                              {"modes", "greedy", "", ""},
                                          });
  assert(noImage);
  assert(noImage->status == 400);
  assert(json::parse(noImage->body)["error"] == "Multipart field 'image' is required");

  auto fakePng = tmp.path / "fake.png";
  {
    std::ofstream out(fakePng);
    out << "definitely not an image";
  }
  auto fakeRes = cli.Post("/api/v1/jobs", httplib::MultipartFormDataItems{
                                              {"image", readFile(fakePng.string()), "fake.png",
                                               "image/png"},
                                          });
  assert(fakeRes);
  assert(fakeRes->status == 400);

  auto input = (tmp.path / "http-input.png").string();
  writeCheckerPng(input, 160);
  std::string pngBytes = readFile(input);
  auto modeRes = cli.Post("/api/v1/jobs", httplib::MultipartFormDataItems{
                                              {"image", pngBytes, "input.png", "image/png"},
                                              {"modes", "nope", "", ""},
                                          });
  assert(modeRes);
  assert(modeRes->status == 400);
  assert(std::string(json::parse(modeRes->body)["error"]).find("Unknown pack mode") !=
         std::string::npos);

  auto created = cli.Post("/api/v1/jobs", httplib::MultipartFormDataItems{
                                              {"image", pngBytes, "input.png", "image/png"},
                                              {"modes", "greedy", "", ""},
                                              {"sizing", "fixed", "", ""},
                                          });
  assert(created);
  assert(created->status == 202);
  auto cj = json::parse(created->body);
  std::string jobId = cj["jobId"];
  assert(isUuid(jobId));
  assert(cj["statusUrl"] == "/api/v1/jobs/" + jobId);

  json job;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (std::chrono::steady_clock::now() < deadline) {
    auto polled = cli.Get("/api/v1/jobs/" + jobId);
    assert(polled);
    assert(polled->status == 200);
    job = json::parse(polled->body);
    std::string st = job["status"];
    if (st == "COMPLETE" || st == "FAILED") {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  assert(job["status"] == "COMPLETE");
  assert(job["result"]["gridWidth"] == 54);
  assert(job["result"]["gridHeight"] == 54);
  assert(job["artifacts"].contains("matched"));
  assert(job["artifacts"].contains("legoGreedy"));
  assert(job["artifacts"].contains("legoStuds"));

  std::string matchedUrl = job["artifacts"]["matched"];
  auto art = cli.Get(matchedUrl);
  assert(art);
  assert(art->status == 200);
  assert(art->get_header_value("X-Content-Type-Options") == "nosniff");

  auto trav = cli.Get("/api/v1/jobs/" + jobId + "/artifacts/../manifest.json");
  assert(trav);
  assert(trav->status == 404);

  auto hidden = cli.Get("/api/v1/jobs/" + jobId + "/artifacts/manifest.json");
  assert(hidden);
  assert(hidden->status == 404);

  auto del = cli.Delete("/api/v1/jobs/" + jobId);
  assert(del);
  assert(del->status == 204);
  auto gone = cli.Get("/api/v1/jobs/" + jobId);
  assert(gone);
  assert(gone->status == 404);

  http.stop();
  serverThread.join();
}

int main() {
  testUploadValidator();
  testPieceTarget();
  testTimeoutFor();
  testFileJobRepoSizing();
  testRecoverInterrupted();
  testPipelineGreedy();
  testPipelineClassicWidth();
  testPipelineStudBom();
  testCatalogParityAndRender();
  testHostHeaderGuard();
  testHttpApi();
  std::cout << "host tests ok\n";
  return 0;
}

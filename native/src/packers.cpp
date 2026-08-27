#include "lego/packers.hpp"

#include "lego/java_random.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace lego {
namespace {

using Covered = std::vector<std::vector<char>>;

uint64_t cellKey(int x, int y) {
  return (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32) ^
         static_cast<uint32_t>(x);
}

int popcount64(uint64_t v) {
  return static_cast<int>(__builtin_popcountll(v));
}

int ctz64(uint64_t v) {
  return v == 0 ? 64 : __builtin_ctzll(v);
}

void tryVisit(const StudGrid& studs, Covered& visited,
              std::queue<std::pair<int, int>>& q, int x, int y, int colorId,
              int w, int h) {
  if (x < 0 || y < 0 || x >= w || y >= h || visited[y][x]) {
    return;
  }
  const LegoElement& el = studs[y][x];
  if (!el.present || el.colorId != colorId) {
    return;
  }
  visited[y][x] = 1;
  q.push({x, y});
}

struct Placement {
  PlateSize size;
  int x = 0;
  int y = 0;
  uint64_t mask = 0;
  int colorId = 0;
  std::string colorName;
};

struct SearchState {
  int bestPieces = 0;
  std::vector<Placement> bestSolution;
  std::vector<Placement> current;
  int64_t deadline = 0;
  bool timedOut = false;
  bool hasBest = false;
};

struct ComponentResult {
  std::vector<PlacedPart> placed;
  bool exact = true;
};

constexpr int EXACT_CELL_LIMIT = 64;
constexpr int64_t COMPONENT_TIME_MS = 30'000;
constexpr int64_t PACK_TIME_MS = 60'000;
constexpr int64_t ANNEAL_TIME_MS = 20'000;
constexpr int ANNEAL_MAX_ITERS = 4000;
constexpr int ANNEAL_WINDOW = 8;

void searchIlp(uint64_t covered, uint64_t full, int pieces,
               const std::vector<Placement>& placements, SearchState& ss) {
  if (nowMs() > ss.deadline) {
    ss.timedOut = true;
    return;
  }
  if (pieces >= ss.bestPieces) {
    return;
  }
  if (covered == full) {
    ss.bestPieces = pieces;
    ss.bestSolution = ss.current;
    ss.hasBest = true;
    return;
  }

  int remaining = popcount64(full & ~covered);
  int maxArea = 1;
  for (const Placement& p : placements) {
    if ((p.mask & covered) == 0) {
      maxArea = std::max(maxArea, p.size.area());
    }
  }
  if (pieces + (remaining + maxArea - 1) / maxArea >= ss.bestPieces) {
    return;
  }

  int bit = ctz64((~covered) & full);
  for (const Placement& p : placements) {
    if (((p.mask >> bit) & 1ULL) == 0) {
      continue;
    }
    if ((p.mask & covered) != 0) {
      continue;
    }
    ss.current.push_back(p);
    searchIlp(covered | p.mask, full, pieces + 1, placements, ss);
    ss.current.pop_back();
    if (ss.timedOut) {
      return;
    }
  }
}

void searchDlx(uint64_t covered, uint64_t full, int pieces, int n,
               const std::vector<Placement>& placements, SearchState& ss) {
  if (nowMs() > ss.deadline) {
    ss.timedOut = true;
    return;
  }
  if (pieces >= ss.bestPieces) {
    return;
  }
  if (covered == full) {
    ss.bestPieces = pieces;
    ss.bestSolution = ss.current;
    ss.hasBest = true;
    return;
  }

  int remaining = popcount64(full & ~covered);
  int maxArea = 1;
  for (const Placement& p : placements) {
    if ((p.mask & covered) == 0) {
      maxArea = std::max(maxArea, p.size.area());
    }
  }
  if (pieces + (remaining + maxArea - 1) / maxArea >= ss.bestPieces) {
    return;
  }

  int bestBit = -1;
  int bestCount = INT32_MAX;
  uint64_t uncovered = full & ~covered;
  for (int bit = 0; bit < n; bit++) {
    if (((uncovered >> bit) & 1ULL) == 0) {
      continue;
    }
    int count = 0;
    for (const Placement& p : placements) {
      if (((p.mask >> bit) & 1ULL) == 0) {
        continue;
      }
      if ((p.mask & covered) != 0) {
        continue;
      }
      count++;
    }
    if (count < bestCount) {
      bestCount = count;
      bestBit = bit;
      if (count == 0) {
        return;
      }
    }
  }
  if (bestBit < 0) {
    return;
  }

  for (const Placement& p : placements) {
    if (((p.mask >> bestBit) & 1ULL) == 0) {
      continue;
    }
    if ((p.mask & covered) != 0) {
      continue;
    }
    ss.current.push_back(p);
    searchDlx(covered | p.mask, full, pieces + 1, n, placements, ss);
    ss.current.pop_back();
    if (ss.timedOut) {
      return;
    }
  }
}

std::vector<Placement> buildPlacements(const PlateCatalog& catalog,
                                       const StudGrid& studs,
                                       const std::vector<std::pair<int, int>>& cells,
                                       int colorId, const std::string& colorName,
                                       int w, int h) {
  std::unordered_map<uint64_t, int> indexOf;
  for (int i = 0; i < static_cast<int>(cells.size()); i++) {
    indexOf[cellKey(cells[i].first, cells[i].second)] = i;
  }
  int n = static_cast<int>(cells.size());
  const auto& sizes = catalog.footprintsForColor(colorId);
  std::vector<Placement> placements;

  for (const auto& cell : cells) {
    int x = cell.first;
    int y = cell.second;
    for (const PlateSize& size : sizes) {
      if (x + size.w > w || y + size.h > h) {
        continue;
      }
      uint64_t mask = 0;
      bool ok = true;
      for (int dy = 0; dy < size.h && ok; dy++) {
        for (int dx = 0; dx < size.w; dx++) {
          auto it = indexOf.find(cellKey(x + dx, y + dy));
          if (it == indexOf.end()) {
            ok = false;
            break;
          }
          mask |= (1ULL << it->second);
        }
      }
      if (ok && popcount64(mask) == size.area()) {
        placements.push_back(Placement{size, x, y, mask, colorId, colorName});
      }
    }
  }

  for (int i = 0; i < n; i++) {
    Placement p;
    p.size = PlateSize{"3024", 1, 1};
    p.x = cells[i].first;
    p.y = cells[i].second;
    p.mask = 1ULL << i;
    p.colorId = colorId;
    p.colorName = colorName;
    placements.push_back(p);
  }

  std::stable_sort(placements.begin(), placements.end(),
                   [](const Placement& a, const Placement& b) {
                     return a.size.area() > b.size.area();
                   });
  return placements;
}

ComponentResult packExactComponent(const PlateCatalog& catalog, const StudGrid& studs,
                                   const std::vector<std::pair<int, int>>& cells,
                                   int colorId, int w, int h, int64_t timeBudgetMs,
                                   bool dlxHeuristic) {
  if (cells.empty()) {
    return {};
  }
  const std::string& colorName = studs[cells[0].second][cells[0].first].colorName;
  if (static_cast<int>(cells.size()) > EXACT_CELL_LIMIT || timeBudgetMs <= 0) {
    return {greedyComponent(catalog, studs, cells, w, h), false};
  }

  int n = static_cast<int>(cells.size());
  auto placements = buildPlacements(catalog, studs, cells, colorId, colorName, w, h);

  SearchState ss;
  ss.bestPieces = n + 1;
  ss.deadline = nowMs() + timeBudgetMs;
  uint64_t full = (n == 64) ? ~0ULL : ((1ULL << n) - 1ULL);
  if (dlxHeuristic) {
    searchDlx(0, full, 0, n, placements, ss);
  } else {
    searchIlp(0, full, 0, placements, ss);
  }

  if (!ss.hasBest) {
    return {greedyComponent(catalog, studs, cells, w, h), false};
  }
  std::vector<PlacedPart> out;
  out.reserve(ss.bestSolution.size());
  for (const Placement& p : ss.bestSolution) {
    out.push_back(PlacedPart{p.size.partNum, p.colorId, p.colorName, p.x, p.y,
                             p.size.w, p.size.h});
  }
  return {out, !ss.timedOut};
}

void placeAt(const PlateCatalog& catalog, const StudGrid& studs, Covered& covered,
             std::vector<PlacedPart>& placed, int x, int y, int w, int h) {
  if (covered[y][x]) {
    return;
  }
  const LegoElement& el = studs[y][x];
  if (!el.present) {
    covered[y][x] = 1;
    return;
  }
  int colorId = el.colorId;
  for (const PlateSize& size : catalog.footprintsForColor(colorId)) {
    if (fits(studs, covered, x, y, size.w, size.h, colorId, w, h)) {
      mark(covered, x, y, size.w, size.h);
      placed.push_back(
          PlacedPart{size.partNum, colorId, el.colorName, x, y, size.w, size.h});
      return;
    }
  }
  covered[y][x] = 1;
  placed.push_back(PlacedPart{"3024", colorId, el.colorName, x, y, 1, 1});
}

std::vector<PlacedPart> packRowMajor(const PlateCatalog& catalog, const StudGrid& studs,
                                     int w, int h, bool topToBottom, bool leftToRight) {
  Covered covered(h, std::vector<char>(w, 0));
  std::vector<PlacedPart> placed;
  int y0 = topToBottom ? 0 : h - 1;
  int y1 = topToBottom ? h : -1;
  int ys = topToBottom ? 1 : -1;
  int x0 = leftToRight ? 0 : w - 1;
  int x1 = leftToRight ? w : -1;
  int xs = leftToRight ? 1 : -1;
  for (int y = y0; y != y1; y += ys) {
    for (int x = x0; x != x1; x += xs) {
      placeAt(catalog, studs, covered, placed, x, y, w, h);
    }
  }
  return placed;
}

std::vector<PlacedPart> packColMajor(const PlateCatalog& catalog, const StudGrid& studs,
                                     int w, int h) {
  Covered covered(h, std::vector<char>(w, 0));
  std::vector<PlacedPart> placed;
  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      placeAt(catalog, studs, covered, placed, x, y, w, h);
    }
  }
  return placed;
}

std::vector<PlacedPart> repairLeftovers(const PlateCatalog& catalog, const StudGrid& studs,
                                        int w, int h, const std::vector<PlacedPart>& initial) {
  Covered covered(h, std::vector<char>(w, 0));
  std::vector<PlacedPart> repaired;
  Covered need(h, std::vector<char>(w, 0));

  for (const PlacedPart& p : initial) {
    if (p.w == 1 && p.h == 1) {
      need[p.y][p.x] = 1;
    } else {
      repaired.push_back(p);
      mark(covered, p.x, p.y, p.w, p.h);
    }
  }

  for (int y = h - 1; y >= 0; y--) {
    for (int x = w - 1; x >= 0; x--) {
      if (need[y][x] && !covered[y][x]) {
        placeAt(catalog, studs, covered, repaired, x, y, w, h);
      }
    }
  }

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (!covered[y][x] && studs[y][x].present) {
        placeAt(catalog, studs, covered, repaired, x, y, w, h);
      }
    }
  }
  return repaired;
}

std::vector<PlateSize> oneHighForColor(const PlateCatalog& catalog, int colorId) {
  std::vector<PlateSize> out;
  for (const PlateSize& p : catalog.footprintsForColor(colorId)) {
    if (p.h == 1) {
      out.push_back(p);
    }
  }
  return out;
}

void emitStripRun(const PlateCatalog& catalog, const StudGrid& studs, Covered& covered,
                  std::vector<PlacedPart>& placed, int x0, int y, int runLen,
                  int colorId, const std::string& colorName, int w, int h) {
  std::vector<PlateSize> strips = oneHighForColor(catalog, colorId);
  int x = x0;
  int remaining = runLen;
  while (remaining > 0) {
    bool placedOne = false;
    for (const PlateSize& size : strips) {
      if (size.w <= remaining &&
          fits(studs, covered, x, y, size.w, size.h, colorId, w, h)) {
        mark(covered, x, y, size.w, size.h);
        placed.push_back(
            PlacedPart{size.partNum, colorId, colorName, x, y, size.w, size.h});
        x += size.w;
        remaining -= size.w;
        placedOne = true;
        break;
      }
    }
    if (!placedOne) {
      mark(covered, x, y, 1, 1);
      placed.push_back(PlacedPart{"3024", colorId, colorName, x, y, 1, 1});
      x++;
      remaining--;
    }
  }
}

const PlateSize* findFootprint(const PlateCatalog& catalog, int colorId, int pw, int ph) {
  for (const PlateSize& s : catalog.footprintsForColor(colorId)) {
    if (s.w == pw && s.h == ph) {
      return &s;
    }
  }
  return nullptr;
}

std::vector<PlacedPart> verticalMerge(const PlateCatalog& catalog,
                                      const std::vector<PlacedPart>& initial) {
  std::vector<PlacedPart> parts = initial;
  bool improved = true;
  while (improved) {
    improved = false;
    bool restart = false;
    for (size_t i = 0; i < parts.size() && !restart; i++) {
      const PlacedPart& a = parts[i];
      for (size_t j = 0; j < parts.size(); j++) {
        if (i == j) {
          continue;
        }
        const PlacedPart& b = parts[j];
        if (a.colorId != b.colorId || a.x != b.x || a.w != b.w) {
          continue;
        }
        if (b.y != a.y + a.h) {
          continue;
        }
        int newH = a.h + b.h;
        const PlateSize* taller = findFootprint(catalog, a.colorId, a.w, newH);
        if (taller == nullptr) {
          continue;
        }
        std::vector<PlacedPart> next;
        next.reserve(parts.size() - 1);
        for (size_t k = 0; k < parts.size(); k++) {
          if (k != i && k != j) {
            next.push_back(parts[k]);
          }
        }
        next.push_back(PlacedPart{taller->partNum, a.colorId, a.colorName, a.x, a.y,
                                  taller->w, taller->h});
        parts = std::move(next);
        improved = true;
        restart = true;
        break;
      }
    }
  }
  return parts;
}

bool partIntersectsWindow(const PlacedPart& p, int wx, int wy, int ww, int wh) {
  return p.x < wx + ww && p.x + p.w > wx && p.y < wy + wh && p.y + p.h > wy;
}

void placeAtShuffled(const PlateCatalog& catalog, JavaRandom& rng, const StudGrid& studs,
                     Covered& covered, std::vector<PlacedPart>& placed, int x, int y,
                     int w, int h) {
  if (covered[y][x]) {
    return;
  }
  const LegoElement& el = studs[y][x];
  if (!el.present) {
    covered[y][x] = 1;
    return;
  }
  int colorId = el.colorId;
  std::vector<PlateSize> sizes = catalog.footprintsForColor(colorId);
  rng.shuffle(sizes);
  for (const PlateSize& size : sizes) {
    if (fits(studs, covered, x, y, size.w, size.h, colorId, w, h)) {
      mark(covered, x, y, size.w, size.h);
      placed.push_back(
          PlacedPart{size.partNum, colorId, el.colorName, x, y, size.w, size.h});
      return;
    }
  }
  covered[y][x] = 1;
  placed.push_back(PlacedPart{"3024", colorId, el.colorName, x, y, 1, 1});
}

std::vector<PlacedPart> rePackWindow(const PlateCatalog& catalog, JavaRandom& rng,
                                     const StudGrid& studs,
                                     const std::vector<PlacedPart>& current, int wx,
                                     int wy, int ww, int wh, int gridW, int gridH) {
  std::vector<PlacedPart> kept;
  Covered covered(gridH, std::vector<char>(gridW, 0));
  for (const PlacedPart& p : current) {
    if (partIntersectsWindow(p, wx, wy, ww, wh)) {
      continue;
    }
    kept.push_back(p);
    mark(covered, p.x, p.y, p.w, p.h);
  }
  for (int y = 0; y < gridH; y++) {
    for (int x = 0; x < gridW; x++) {
      if (!studs[y][x].present) {
        covered[y][x] = 1;
      }
    }
  }

  std::vector<PlacedPart> result = kept;
  for (int y = wy; y < wy + wh; y++) {
    for (int x = wx; x < wx + ww; x++) {
      if (covered[y][x]) {
        continue;
      }
      placeAtShuffled(catalog, rng, studs, covered, result, x, y, gridW, gridH);
    }
  }
  for (int y = 0; y < gridH; y++) {
    for (int x = 0; x < gridW; x++) {
      if (!covered[y][x] && studs[y][x].present) {
        placeAtShuffled(catalog, rng, studs, covered, result, x, y, gridW, gridH);
      }
    }
  }
  return result;
}

}  // namespace

bool fits(const StudGrid& studs, const Covered& covered, int x, int y, int pw, int ph,
          int colorId, int w, int h) {
  if (x + pw > w || y + ph > h) {
    return false;
  }
  for (int dy = 0; dy < ph; dy++) {
    for (int dx = 0; dx < pw; dx++) {
      if (covered[y + dy][x + dx]) {
        return false;
      }
      const LegoElement& el = studs[y + dy][x + dx];
      if (!el.present || el.colorId != colorId) {
        return false;
      }
    }
  }
  return true;
}

void mark(Covered& covered, int x, int y, int pw, int ph) {
  for (int dy = 0; dy < ph; dy++) {
    for (int dx = 0; dx < pw; dx++) {
      covered[y + dy][x + dx] = 1;
    }
  }
}

std::vector<std::pair<int, int>> flood(const StudGrid& studs, Covered& visited, int sx,
                                       int sy, int colorId, int w, int h) {
  std::vector<std::pair<int, int>> cells;
  std::queue<std::pair<int, int>> q;
  q.push({sx, sy});
  visited[sy][sx] = 1;
  while (!q.empty()) {
    auto c = q.front();
    q.pop();
    cells.push_back(c);
    tryVisit(studs, visited, q, c.first + 1, c.second, colorId, w, h);
    tryVisit(studs, visited, q, c.first - 1, c.second, colorId, w, h);
    tryVisit(studs, visited, q, c.first, c.second + 1, colorId, w, h);
    tryVisit(studs, visited, q, c.first, c.second - 1, colorId, w, h);
  }
  return cells;
}

std::vector<PlacedPart> greedyComponent(const PlateCatalog& catalog, const StudGrid& studs,
                                        const std::vector<std::pair<int, int>>& cells,
                                        int w, int h) {
  Covered inComp(h, std::vector<char>(w, 0));
  for (const auto& c : cells) {
    inComp[c.second][c.first] = 1;
  }
  Covered covered(h, std::vector<char>(w, 0));
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (!inComp[y][x]) {
        covered[y][x] = 1;
      }
    }
  }

  std::vector<PlacedPart> placed;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (covered[y][x]) {
        continue;
      }
      const LegoElement& el = studs[y][x];
      int colorId = el.colorId;
      bool placedOne = false;
      for (const PlateSize& size : catalog.footprintsForColor(colorId)) {
        if (!fits(studs, covered, x, y, size.w, size.h, colorId, w, h)) {
          continue;
        }
        bool ok = true;
        for (int dy = 0; dy < size.h && ok; dy++) {
          for (int dx = 0; dx < size.w; dx++) {
            if (!inComp[y + dy][x + dx]) {
              ok = false;
              break;
            }
          }
        }
        if (!ok) {
          continue;
        }
        mark(covered, x, y, size.w, size.h);
        placed.push_back(
            PlacedPart{size.partNum, colorId, el.colorName, x, y, size.w, size.h});
        placedOne = true;
        break;
      }
      if (!placedOne) {
        covered[y][x] = 1;
        placed.push_back(PlacedPart{"3024", colorId, el.colorName, x, y, 1, 1});
      }
    }
  }
  return placed;
}

PackResult packGreedy(const PlateCatalog& catalog, const StudGrid& studs) {
  int64_t t0 = nowMs();
  if (studs.empty() || studs[0].empty()) {
    return PackResult{"greedy", {}, 0, "ok"};
  }
  int h = static_cast<int>(studs.size());
  int w = static_cast<int>(studs[0].size());

  std::vector<std::vector<PlacedPart>> candidates;
  candidates.push_back(packRowMajor(catalog, studs, w, h, true, true));
  candidates.push_back(packRowMajor(catalog, studs, w, h, true, false));
  candidates.push_back(packRowMajor(catalog, studs, w, h, false, true));
  candidates.push_back(packColMajor(catalog, studs, w, h));

  std::vector<PlacedPart> best;
  bool have = false;
  for (const auto& candidate : candidates) {
    auto repaired = repairLeftovers(catalog, studs, w, h, candidate);
    if (!have || repaired.size() < best.size()) {
      best = std::move(repaired);
      have = true;
    }
  }
  return PackResult{"greedy", best, nowMs() - t0, "ok"};
}

PackResult packRle(const PlateCatalog& catalog, const StudGrid& studs) {
  int64_t t0 = nowMs();
  if (studs.empty() || studs[0].empty()) {
    return PackResult{"rle", {}, 0, "ok"};
  }
  int h = static_cast<int>(studs.size());
  int w = static_cast<int>(studs[0].size());
  Covered covered(h, std::vector<char>(w, 0));
  std::vector<PlacedPart> placed;

  for (int y = 0; y < h; y++) {
    int x = 0;
    while (x < w) {
      if (covered[y][x] || !studs[y][x].present) {
        if (!studs[y][x].present) {
          covered[y][x] = 1;
        }
        x++;
        continue;
      }
      int colorId = studs[y][x].colorId;
      std::string colorName = studs[y][x].colorName;
      int start = x;
      while (x < w && !covered[y][x] && studs[y][x].present &&
             studs[y][x].colorId == colorId) {
        x++;
      }
      int runLen = x - start;
      emitStripRun(catalog, studs, covered, placed, start, y, runLen, colorId,
                   colorName, w, h);
    }
  }

  placed = verticalMerge(catalog, placed);
  return PackResult{"rle", placed, nowMs() - t0, "ok"};
}

PackResult packComponent(const PlateCatalog& catalog, const StudGrid& studs) {
  int64_t t0 = nowMs();
  if (studs.empty() || studs[0].empty()) {
    return PackResult{"component", {}, 0, "ok"};
  }
  int h = static_cast<int>(studs.size());
  int w = static_cast<int>(studs[0].size());
  Covered visited(h, std::vector<char>(w, 0));
  std::vector<PlacedPart> all;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (visited[y][x] || !studs[y][x].present) {
        continue;
      }
      int colorId = studs[y][x].colorId;
      auto cells = flood(studs, visited, x, y, colorId, w, h);
      auto more = greedyComponent(catalog, studs, cells, w, h);
      all.insert(all.end(), more.begin(), more.end());
    }
  }
  return PackResult{"component", all, nowMs() - t0, "ok"};
}

static PackResult packExact(const PlateCatalog& catalog, const StudGrid& studs,
                            const std::string& modeName, bool dlxHeuristic) {
  int64_t t0 = nowMs();
  int64_t packDeadline = t0 + PACK_TIME_MS;
  if (studs.empty() || studs[0].empty()) {
    return PackResult{modeName, {}, 0, "ok"};
  }
  int h = static_cast<int>(studs.size());
  int w = static_cast<int>(studs[0].size());
  Covered visited(h, std::vector<char>(w, 0));
  std::vector<PlacedPart> all;
  int exactComponents = 0;
  int fallbackComponents = 0;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      if (visited[y][x] || !studs[y][x].present) {
        continue;
      }
      int colorId = studs[y][x].colorId;
      auto cells = flood(studs, visited, x, y, colorId, w, h);
      int64_t budget = std::min(COMPONENT_TIME_MS, std::max(int64_t{0}, packDeadline - nowMs()));
      auto cr = packExactComponent(catalog, studs, cells, colorId, w, h, budget, dlxHeuristic);
      all.insert(all.end(), cr.placed.begin(), cr.placed.end());
      if (cr.exact) {
        exactComponents++;
      } else {
        fallbackComponents++;
      }
    }
  }
  std::string status = fallbackComponents == 0
                           ? "optimal"
                           : "exact_partial (" + std::to_string(exactComponents) +
                                 " exact blobs, " + std::to_string(fallbackComponents) +
                                 " greedy fallback)";
  return PackResult{modeName, all, nowMs() - t0, status};
}

PackResult packIlp(const PlateCatalog& catalog, const StudGrid& studs) {
  return packExact(catalog, studs, "ilp", false);
}

PackResult packDlx(const PlateCatalog& catalog, const StudGrid& studs) {
  return packExact(catalog, studs, "dlx", true);
}

PackResult packAnneal(const PlateCatalog& catalog, const StudGrid& studs) {
  int64_t t0 = nowMs();
  int64_t deadline = t0 + ANNEAL_TIME_MS;
  if (studs.empty() || studs[0].empty()) {
    return PackResult{"anneal", {}, 0, "ok"};
  }

  PackResult seed = packGreedy(catalog, studs);
  std::vector<PlacedPart> best = seed.placed;
  std::vector<PlacedPart> current = seed.placed;
  int bestCount = static_cast<int>(best.size());
  int currentCount = static_cast<int>(current.size());
  int seedCount = seed.pieceCount();
  int h = static_cast<int>(studs.size());
  int w = static_cast<int>(studs[0].size());
  double temperature = std::max(5.0, seedCount * 0.02);
  JavaRandom rng(42);

  for (int iter = 0; iter < ANNEAL_MAX_ITERS; iter++) {
    if (nowMs() > deadline) {
      break;
    }
    int wx = rng.nextInt(w);
    int wy = rng.nextInt(h);
    int ww = std::min(ANNEAL_WINDOW, w - wx);
    int wh = std::min(ANNEAL_WINDOW, h - wy);
    if (ww <= 0 || wh <= 0) {
      continue;
    }
    auto candidate = rePackWindow(catalog, rng, studs, current, wx, wy, ww, wh, w, h);
    int candCount = static_cast<int>(candidate.size());
    int delta = candCount - currentCount;
    bool accept = false;
    if (delta <= 0) {
      accept = true;
    } else if (temperature > 1e-9) {
      accept = rng.nextDouble() < std::exp(-delta / temperature);
    }
    if (accept) {
      current = std::move(candidate);
      currentCount = candCount;
      if (candCount < bestCount) {
        best = current;
        bestCount = candCount;
      }
    }
    temperature *= 0.995;
  }

  std::string status = bestCount < seedCount ? "improved" : "ok";
  return PackResult{"anneal", best, nowMs() - t0, status};
}

PackResult pack(const std::string& mode, const PlateCatalog& catalog, const StudGrid& studs) {
  if (mode == "greedy") {
    return packGreedy(catalog, studs);
  }
  if (mode == "ilp") {
    return packIlp(catalog, studs);
  }
  if (mode == "rle") {
    return packRle(catalog, studs);
  }
  if (mode == "component") {
    return packComponent(catalog, studs);
  }
  if (mode == "dlx") {
    return packDlx(catalog, studs);
  }
  if (mode == "anneal") {
    return packAnneal(catalog, studs);
  }
  throw std::invalid_argument("unknown pack mode: " + mode);
}

}  // namespace lego

#include "lego/text.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lego {

std::string capitalize(const std::string& s) {
  if (s.empty()) {
    return s;
  }
  std::string out = s;
  if (out[0] >= 'a' && out[0] <= 'z') {
    out[0] = static_cast<char>(out[0] - 'a' + 'A');
  }
  return out;
}

void writeLines(const std::string& path, const std::vector<std::string>& lines) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("could not write " + path);
  }
  for (size_t i = 0; i < lines.size(); i++) {
    out << lines[i] << '\n';
  }
}

static std::string bomLine(int count, const std::string& part, int w, int h, int colorId,
                           const std::string& name) {
  std::ostringstream os;
  os << "  " << count << "  " << part << " " << w << "x" << h << "  color " << colorId << " "
     << name;
  return os.str();
}

std::vector<std::string> formatStudBom(const std::vector<BomRow>& studBom) {
  std::vector<std::string> lines;
  int total = 0;
  for (const auto& row : studBom) {
    total += row.count;
  }
  lines.push_back("Mode: studs");
  lines.push_back("Status: 1x1 per stud");
  lines.push_back("Total pieces: " + std::to_string(total));
  lines.push_back("By part and color:");
  for (const auto& row : studBom) {
    lines.push_back(bomLine(row.count, row.partNum, row.w, row.h, row.colorId, row.colorName));
  }
  return lines;
}

std::vector<BomRow> bomRows(const PackResult& result) {
  std::map<std::string, int> counts;
  std::map<std::string, PlacedPart> samples;
  for (const PlacedPart& p : result.placed) {
    std::string key = p.partNum + "|" + std::to_string(p.colorId);
    counts[key] += 1;
    if (!samples.count(key)) {
      samples.emplace(key, p);
    }
  }
  std::vector<std::string> keys;
  keys.reserve(counts.size());
  for (const auto& kv : counts) {
    keys.push_back(kv.first);
  }
  std::sort(keys.begin(), keys.end(), [&](const std::string& a, const std::string& b) {
    int ca = counts[a];
    int cb = counts[b];
    if (ca != cb) {
      return ca > cb;
    }
    const auto& sa = samples[a];
    const auto& sb = samples[b];
    if (sa.colorName != sb.colorName) {
      return sa.colorName < sb.colorName;
    }
    return sa.partNum < sb.partNum;
  });
  std::vector<BomRow> rows;
  for (const auto& key : keys) {
    const PlacedPart& s = samples[key];
    rows.push_back(BomRow{s.partNum, s.w, s.h, s.colorId, s.colorName, counts[key]});
  }
  return rows;
}

std::vector<std::string> formatBom(const PackResult& result) {
  std::vector<std::string> lines;
  lines.push_back("Mode: " + result.modeName);
  lines.push_back("Status: " + result.status);
  lines.push_back("Time ms: " + std::to_string(result.elapsedMs));
  lines.push_back("Total pieces: " + std::to_string(result.pieceCount()));
  lines.push_back("By part and color:");
  for (const auto& row : bomRows(result)) {
    lines.push_back(bomLine(row.count, row.partNum, row.w, row.h, row.colorId, row.colorName));
  }
  return lines;
}

std::vector<BomRow> studBomRows(const std::map<int, int>& colorCounts,
                                const std::map<int, LegoElement>& colorSamples) {
  std::vector<BomRow> rows;
  for (const auto& e : colorCounts) {
    auto it = colorSamples.find(e.first);
    std::string part = it != colorSamples.end() ? it->second.partNum : DEFAULT_STUD_PART;
    std::string name = it != colorSamples.end() ? it->second.colorName : "";
    rows.push_back(BomRow{part, 1, 1, e.first, name, e.second});
  }
  std::sort(rows.begin(), rows.end(), [](const BomRow& a, const BomRow& b) {
    if (a.count != b.count) {
      return a.count > b.count;
    }
    if (a.colorName != b.colorName) {
      return a.colorName < b.colorName;
    }
    return a.partNum < b.partNum;
  });
  return rows;
}

std::vector<ColorCountRow> colorCountRows(const std::map<int, int>& colorCounts,
                                          const std::map<int, LegoElement>& colorSamples) {
  std::vector<ColorCountRow> rows;
  for (const auto& e : colorCounts) {
    auto it = colorSamples.find(e.first);
    rows.push_back(ColorCountRow{
        e.first,
        it != colorSamples.end() ? it->second.colorName : "",
        it != colorSamples.end() ? it->second.rgbHex : "",
        e.second});
  }
  std::sort(rows.begin(), rows.end(), [](const ColorCountRow& a, const ColorCountRow& b) {
    if (a.count != b.count) {
      return a.count > b.count;
    }
    return a.colorName < b.colorName;
  });
  return rows;
}

std::vector<std::string> formatColorCounts(const std::map<int, int>& colorCounts,
                                           const std::map<int, LegoElement>& colorSamples) {
  std::vector<std::string> lines;
  int total = 0;
  for (const auto& e : colorCounts) {
    total += e.second;
  }
  lines.push_back("Total pieces: " + std::to_string(total));
  auto rows = colorCountRows(colorCounts, colorSamples);
  for (const auto& row : rows) {
    std::ostringstream os;
    if (row.colorName.empty() && row.rgbHex.empty()) {
      os << "  " << row.count << "  color " << row.colorId;
    } else {
      os << "  " << row.count << "  color " << row.colorId << " " << row.colorName << " (#"
         << row.rgbHex << ")  part "
         << (colorSamples.count(row.colorId) ? colorSamples.at(row.colorId).partNum
                                             : DEFAULT_STUD_PART);
    }
    lines.push_back(os.str());
  }
  return lines;
}

}  // namespace lego

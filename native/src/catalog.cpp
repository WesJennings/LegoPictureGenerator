#include "lego/catalog.hpp"

#include <algorithm>
#include <filesystem>
#include <cctype>
#include <set>
#include <sqlite3.h>
#include <stdexcept>
#include <unordered_set>

namespace lego {
namespace {

struct Sqlite {
  sqlite3* db = nullptr;
  Sqlite(const std::string& path, int flags) {
    if (path.find('\0') != std::string::npos) {
      throw std::invalid_argument("invalid database path");
    }
    int rc = sqlite3_open_v2(path.c_str(), &db, flags, nullptr);
    if (rc != SQLITE_OK) {
      std::string err = db ? sqlite3_errmsg(db) : "sqlite open failed";
      if (db) {
        sqlite3_close(db);
        db = nullptr;
      }
      throw std::runtime_error("Could not open bricks.db: " + err);
    }
    sqlite3_busy_timeout(db, 2000);
    sqlite3_extended_result_codes(db, 1);
  }
  Sqlite(const Sqlite&) = delete;
  Sqlite& operator=(const Sqlite&) = delete;
  ~Sqlite() {
    if (db) {
      sqlite3_close(db);
    }
  }
};

struct Stmt {
  sqlite3_stmt* s = nullptr;
  Stmt(sqlite3* db, const char* sql) {
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) {
      throw std::runtime_error(std::string("SQL prepare failed: ") + sqlite3_errmsg(db));
    }
  }
  Stmt(const Stmt&) = delete;
  Stmt& operator=(const Stmt&) = delete;
  ~Stmt() {
    if (s) {
      sqlite3_finalize(s);
    }
  }
};

const PlateSize kBase[] = {
    {"41539", 8, 8}, {"3028", 6, 12},  {"3029", 4, 12},  {"3033", 6, 10},
    {"3030", 4, 10}, {"3036", 6, 8},   {"3035", 4, 8},   {"2445", 2, 12},
    {"3958", 6, 6},  {"3032", 4, 6},   {"60479", 1, 12}, {"3832", 2, 10},
    {"3031", 4, 4},  {"4477", 1, 10},  {"3034", 2, 8},   {"3795", 2, 6},
    {"3460", 1, 8},  {"3666", 1, 6},   {"3020", 2, 4},   {"3710", 1, 4},
    {"3021", 2, 3},  {"3623", 1, 3},   {"3022", 2, 2},   {"3023", 1, 2},
    {"3024", 1, 1},
};

void addOriented(std::vector<PlateSize>& list, std::unordered_set<std::string>& dedupe,
                 const std::string& partNum, int w, int h) {
  std::string k1 = partNum + ":" + std::to_string(w) + "x" + std::to_string(h);
  if (dedupe.insert(k1).second) {
    list.push_back(PlateSize{partNum, w, h});
  }
  if (w != h) {
    std::string k2 = partNum + ":" + std::to_string(h) + "x" + std::to_string(w);
    if (dedupe.insert(k2).second) {
      list.push_back(PlateSize{partNum, h, w});
    }
  }
}

std::vector<PlateSize> buildFootprints() {
  std::vector<PlateSize> list;
  std::unordered_set<std::string> dedupe;
  for (const PlateSize& base : kBase) {
    addOriented(list, dedupe, base.partNum, base.w, base.h);
  }
  std::stable_sort(list.begin(), list.end(), [](const PlateSize& a, const PlateSize& b) {
    if (a.area() != b.area()) {
      return a.area() > b.area();
    }
    return std::max(a.w, a.h) > std::max(b.w, b.h);
  });
  return list;
}

int hexByte(const std::string& hex, size_t off) {
  if (off + 2 > hex.size()) {
    throw std::invalid_argument("rgb hex too short");
  }
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
      return 10 + (c - 'A');
    }
    throw std::invalid_argument("invalid rgb hex");
  };
  return (nibble(hex[off]) << 4) | nibble(hex[off + 1]);
}

}  // namespace

bool isTransparentFlag(const std::string& isTrans) {
  std::string v;
  v.reserve(isTrans.size());
  for (char c : isTrans) {
    if (!std::isspace(static_cast<unsigned char>(c))) {
      v.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
  }
  return v == "t" || v == "true";
}

LegoElement makeElement(const std::string& partNum, int colorId,
                        const std::string& colorName, const std::string& rgbHex) {
  LegoElement el;
  el.partNum = partNum;
  el.colorId = colorId;
  el.colorName = colorName;
  el.rgbHex = rgbHex;
  el.r = hexByte(rgbHex, 0);
  el.g = hexByte(rgbHex, 2);
  el.b = hexByte(rgbHex, 4);
  el.present = true;
  return el;
}

std::vector<PaletteEntry> paletteRgb(const std::vector<LegoElement>& palette) {
  std::vector<PaletteEntry> out;
  out.reserve(palette.size());
  for (const auto& e : palette) {
    out.push_back(PaletteEntry{e.r, e.g, e.b});
  }
  return out;
}

Catalog loadCatalog(const std::string& dbPath) {
  if (dbPath.find('\0') != std::string::npos) {
    throw std::invalid_argument("invalid database path");
  }
  if (!std::filesystem::is_regular_file(dbPath)) {
    throw std::runtime_error("bricks.db not found at " + dbPath +
                             " — see data/README.md for how to obtain it");
  }
  Catalog cat;
  cat.dbPath = dbPath;
  Sqlite db(dbPath, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX);

  {
    Stmt st(db.db,
            "SELECT DISTINCT e.part_num, c.id AS color_id, c.name AS color_name, c.rgb, c.is_trans "
            "FROM elements e JOIN colors c ON c.id = e.color_id "
            "WHERE e.part_num = ? AND c.id >= 0 ORDER BY c.id");
    sqlite3_bind_text(st.s, 1, DEFAULT_STUD_PART, -1, SQLITE_STATIC);
    while (sqlite3_step(st.s) == SQLITE_ROW) {
      const unsigned char* trans = sqlite3_column_text(st.s, 4);
      std::string isTrans = trans ? reinterpret_cast<const char*>(trans) : "";
      if (isTransparentFlag(isTrans)) {
        continue;
      }
      const unsigned char* part = sqlite3_column_text(st.s, 0);
      const unsigned char* name = sqlite3_column_text(st.s, 2);
      const unsigned char* rgb = sqlite3_column_text(st.s, 3);
      if (!part || !name || !rgb) {
        continue;
      }
      cat.palette.push_back(makeElement(reinterpret_cast<const char*>(part),
                                        sqlite3_column_int(st.s, 1),
                                        reinterpret_cast<const char*>(name),
                                        reinterpret_cast<const char*>(rgb)));
    }
  }
  if (cat.palette.empty()) {
    throw std::runtime_error(std::string("No opaque elements found for part ") + DEFAULT_STUD_PART);
  }

  std::unordered_map<std::string, std::unordered_set<int>> colorsByPart;
  {
    Stmt st(db.db, "SELECT DISTINCT part_num, color_id FROM elements WHERE part_num = ?");
    std::unordered_set<std::string> seen;
    for (const PlateSize& base : kBase) {
      if (!seen.insert(base.partNum).second) {
        continue;
      }
      sqlite3_reset(st.s);
      sqlite3_clear_bindings(st.s);
      sqlite3_bind_text(st.s, 1, base.partNum.c_str(), -1, SQLITE_TRANSIENT);
      std::unordered_set<int> colors;
      while (sqlite3_step(st.s) == SQLITE_ROW) {
        colors.insert(sqlite3_column_int(st.s, 1));
      }
      colorsByPart[base.partNum] = std::move(colors);
    }
  }

  auto footprints = buildFootprints();
  std::set<int> allColors;
  for (const auto& e : cat.palette) {
    allColors.insert(e.colorId);
  }
  for (const auto& kv : colorsByPart) {
    allColors.insert(kv.second.begin(), kv.second.end());
  }
  for (int colorId : allColors) {
    std::vector<PlateSize> fp;
    for (const PlateSize& p : footprints) {
      auto it = colorsByPart.find(p.partNum);
      if (it != colorsByPart.end() && it->second.count(colorId)) {
        fp.push_back(p);
      }
    }
    cat.plates.byColor[colorId] = std::move(fp);
  }
  return cat;
}

}  // namespace lego

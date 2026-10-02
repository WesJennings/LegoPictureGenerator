#include "lego/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace lego {
namespace {

int clampByte(int v) { return std::max(0, std::min(255, v)); }

uint32_t shade(uint32_t argb, float factor) {
  int a = (argb >> 24) & 0xFF;
  int r = clampByte(static_cast<int>(((argb >> 16) & 0xFF) * factor));
  int g = clampByte(static_cast<int>(((argb >> 8) & 0xFF) * factor));
  int b = clampByte(static_cast<int>((argb & 0xFF) * factor));
  return (static_cast<uint32_t>(a) << 24) | (r << 16) | (g << 8) | b;
}

struct Canvas {
  int width = 0;
  int height = 0;
  std::vector<uint32_t> px;

  void fillRect(int x, int y, int w, int h, uint32_t color) {
    int x1 = std::max(0, x);
    int y1 = std::max(0, y);
    int x2 = std::min(width, x + w);
    int y2 = std::min(height, y + h);
    for (int yy = y1; yy < y2; yy++) {
      for (int xx = x1; xx < x2; xx++) {
        px[yy * width + xx] = color;
      }
    }
  }

  // Ellipse inscribed in [x, x+w) x [y, y+h). Coverage-style AA for knobs.
  void fillEllipse(int x, int y, int w, int h, uint32_t color, bool aa = true) {
    if (w <= 0 || h <= 0) {
      return;
    }
    double cx = x + w / 2.0;
    double cy = y + h / 2.0;
    double rx = w / 2.0;
    double ry = h / 2.0;
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(width, x + w);
    int y1 = std::min(height, y + h);
    for (int yy = y0; yy < y1; yy++) {
      for (int xx = x0; xx < x1; xx++) {
        double nx = (xx + 0.5 - cx) / rx;
        double ny = (yy + 0.5 - cy) / ry;
        double d = nx * nx + ny * ny;
        if (d <= 1.0) {
          if (!aa || d <= 0.85) {
            px[yy * width + xx] = color;
          } else {
            double t = (1.0 - d) / 0.15;
            px[yy * width + xx] = blend(px[yy * width + xx], color, t);
          }
        }
      }
    }
  }

  // Java fillArc: 0° = 3 o'clock, CCW positive.
  void fillArc(int x, int y, int w, int h, int startDeg, int extentDeg, uint32_t color) {
    if (w <= 0 || h <= 0) {
      return;
    }
    double cx = x + w / 2.0;
    double cy = y + h / 2.0;
    double rx = w / 2.0;
    double ry = h / 2.0;
    constexpr double kPi = 3.14159265358979323846;
    double a0 = startDeg * kPi / 180.0;
    double a1 = (startDeg + extentDeg) * kPi / 180.0;
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(width, x + w);
    int y1 = std::min(height, y + h);
    for (int yy = y0; yy < y1; yy++) {
      for (int xx = x0; xx < x1; xx++) {
        double dx = xx + 0.5 - cx;
        double dy = yy + 0.5 - cy;
        double nx = dx / rx;
        double ny = dy / ry;
        if (nx * nx + ny * ny > 1.0) {
          continue;
        }
        // Java y grows down; atan2(-dy, dx) matches math CCW with y-up.
        double ang = std::atan2(-dy, dx);
        if (ang < 0) {
          ang += 2 * kPi;
        }
        double from = a0;
        double to = a1;
        while (from < 0) {
          from += 2 * kPi;
        }
        while (to < from) {
          to += 2 * kPi;
        }
        double a = ang;
        if (a < from) {
          a += 2 * kPi;
        }
        if (a >= from && a <= to) {
          px[yy * width + xx] = color;
        }
      }
    }
  }

  static uint32_t blend(uint32_t dst, uint32_t src, double t) {
    t = std::max(0.0, std::min(1.0, t));
    auto ch = [&](int shift) {
      int s = (src >> shift) & 0xFF;
      int d = (dst >> shift) & 0xFF;
      return static_cast<int>(d + (s - d) * t);
    };
    return (static_cast<uint32_t>(ch(24)) << 24) | (ch(16) << 16) | (ch(8) << 8) | ch(0);
  }
};

void drawKnob(Canvas& g, int ox, int oy, int size, uint32_t base) {
  uint32_t knob = shade(base, 1.18f);
  uint32_t knobShadow = shade(base, 0.85f);
  uint32_t knobHighlight = shade(base, 1.35f);

  int knobSize = std::max(4, static_cast<int>(size * 0.60));
  int kx = ox + (size - knobSize) / 2;
  int ky = oy + (size - knobSize) / 2;

  g.fillEllipse(kx, ky, knobSize, knobSize, knob);
  g.fillArc(kx, ky + knobSize / 3, knobSize, (knobSize * 2) / 3, 200, 140, knobShadow);
  int hi = std::max(2, knobSize / 3);
  g.fillEllipse(kx + knobSize / 4, ky + knobSize / 6, hi, hi, knobHighlight);
}

void drawPlate(Canvas& g, int ox, int oy, int studsW, int studsH, int studSize,
               uint32_t argb) {
  uint32_t base = argb;
  uint32_t edge = shade(base, 0.62f);
  uint32_t rim = shade(base, 0.88f);
  int pw = studsW * studSize;
  int ph = studsH * studSize;

  g.fillRect(ox, oy, pw, ph, base);
  g.fillRect(ox, oy, pw, 1, rim);
  g.fillRect(ox, oy, 1, ph, rim);
  g.fillRect(ox + pw - 2, oy, 2, ph, edge);
  g.fillRect(ox, oy + ph - 2, pw, 2, edge);

  for (int sy = 0; sy < studsH; sy++) {
    for (int sx = 0; sx < studsW; sx++) {
      drawKnob(g, ox + sx * studSize, oy + sy * studSize, studSize, base);
    }
  }
}

}  // namespace

std::vector<int> renderStuds(const int* gridArgb, int cols, int rows, int studSizePx) {
  if (studSizePx < 4) {
    throw std::invalid_argument("studSizePx must be >= 4");
  }
  Canvas g;
  g.width = cols * studSizePx;
  g.height = rows * studSizePx;
  g.px.assign(static_cast<size_t>(g.width) * g.height, 0);
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      drawPlate(g, x * studSizePx, y * studSizePx, 1, 1, studSizePx,
                static_cast<uint32_t>(gridArgb[y * cols + x]));
    }
  }
  return std::vector<int>(g.px.begin(), g.px.end());
}

std::vector<int> renderPacked(const int* gridArgb, int cols, int rows, int studSizePx,
                              const std::vector<PlacedPart>& placed) {
  if (studSizePx < 4) {
    throw std::invalid_argument("studSizePx must be >= 4");
  }
  if (cols <= 0 || rows <= 0) {
    throw std::invalid_argument("studs grid must be non-empty");
  }
  Canvas g;
  g.width = cols * studSizePx;
  g.height = rows * studSizePx;
  g.px.assign(static_cast<size_t>(g.width) * g.height, 0);
  g.fillRect(0, 0, g.width, g.height, 0xFF2A2A2A);

  for (const PlacedPart& p : placed) {
    if (p.w <= 0 || p.h <= 0) {
      continue;
    }
    if (p.y < 0 || p.x < 0 || p.y >= rows || p.x >= cols) {
      continue;
    }
    uint32_t argb = static_cast<uint32_t>(gridArgb[p.y * cols + p.x]);
    drawPlate(g, p.x * studSizePx, p.y * studSizePx, p.w, p.h, studSizePx, argb);
  }
  return std::vector<int>(g.px.begin(), g.px.end());
}

}  // namespace lego

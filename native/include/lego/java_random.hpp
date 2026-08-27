#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace lego {

/**
 * java.util.Random (OpenJDK LCG), so AnnealPacker shuffle/nextInt/nextDouble
 * match the Java seed-42 sequence exactly.
 */
class JavaRandom {
 public:
  explicit JavaRandom(int64_t seed) { setSeed(seed); }

  void setSeed(int64_t seed) {
    seed_ = (seed ^ kMultiplier) & kMask;
  }

  int nextInt(int bound) {
    if (bound <= 0) {
      return 0;
    }
    if ((bound & -bound) == bound) {
      return static_cast<int>((bound * static_cast<int64_t>(next(31))) >> 31);
    }
    int bits;
    int val;
    do {
      bits = next(31);
      val = bits % bound;
    } while (bits - val + (bound - 1) < 0);
    return val;
  }

  double nextDouble() {
    return ((static_cast<int64_t>(next(26)) << 27) + next(27)) * 0x1.0p-53;
  }

  template <typename T>
  void shuffle(std::vector<T>& list) {
    for (int i = static_cast<int>(list.size()); i > 1; i--) {
      int j = nextInt(i);
      std::swap(list[i - 1], list[j]);
    }
  }

 private:
  static constexpr int64_t kMultiplier = 0x5DEECE66DLL;
  static constexpr int64_t kAddend = 0xBLL;
  static constexpr int64_t kMask = (1LL << 48) - 1;

  int next(int bits) {
    seed_ = (seed_ * kMultiplier + kAddend) & kMask;
    return static_cast<int>(seed_ >> (48 - bits));
  }

  int64_t seed_ = 0;
};

}  // namespace lego

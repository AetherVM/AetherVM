// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <format>
#include <iomanip>
#include <iostream>
#include <string_view>

#include "Common.h"

namespace fs = std::filesystem;

namespace aether {

// branchless binary search over a sorted contiguous array
template <typename T>
const T *binary_search(const T *arr, size_t size, const T &key) {
  const T *base = arr;
  while (size > 1) {
    size_t half = size / 2;
    // conditional assignment compiled into branchless instructions (cmov)
    base = (base[half - 1] < key) ? (base + half) : base;
    size = size - half;
  }
  return (*base < key) ? base + 1 : base;
}

template <typename T>
inline bool is_exact(const T *found, const T *arr, size_t size, const T &key) {
  return found != arr + size && *found == key;
}

template <typename T> constexpr T align_up(T value, size_t align_size) {
  return (value + align_size - 1) & ~(align_size - 1);
}

template <typename T> constexpr T align_down(T value, size_t align_size) {
  return value & ~(align_size - 1);
}

enum LogType {
  Develop,
  Runtime,
  Ignore,
};

template <typename... Args>
inline void log_print(LogType type, std::format_string<Args...> format,
                      Args &&...args) {
  bool commit = false;
  char tchar = ' ';
  switch (type) {
  case Develop:
#if AETHER_DEBUG
    commit = true;
    tchar = 'D';
#endif
    break;
  case Runtime:
    commit = true;
    tchar = 'R';
    break;
  case Ignore:
    return;
  default:
    return;
  }
  if (!commit)
    return;

  auto now = std::time(nullptr);
  std::cout << std::put_time(std::localtime(&now), "%T") << " " << tchar
            << " - ";
  auto msg = std::vformat(format.get(), std::make_format_args(args...));
  std::cout << msg << std::endl;
}

inline uint8_t fib_hash8(uint8_t x) { return x; }

inline uint8_t fib_hash8(uint16_t x) {
  constexpr uint16_t K16 = 40503U;
  return static_cast<uint8_t>((x * K16) >> 8);
}

inline uint8_t fib_hash8(uint32_t x) {
  constexpr uint32_t K32 = 2654435769U;
  return static_cast<uint8_t>((x * K32) >> 24);
}

inline uint8_t fib_hash8(uint64_t x) {
  constexpr uint64_t K64 = 11400714819323198485ULL;
  return static_cast<uint8_t>((x * K64) >> 56);
}

struct uint128_var_t {
  uint64_t low, high;

  auto operator<=>(const uint128_var_t &right) const {
    if (auto cmp = high <=> right.high; cmp != 0)
      return cmp;
    return low <=> right.low;
  }

  bool operator==(const uint128_var_t &right) const = default;
};

inline uint8_t fib_hash8(uint128_var_t x) {
  constexpr uint64_t K64 = 11400714819323198485ULL;

  // combine both 64-bit words via XOR and perform a single 64-bit
  // multiplication
  uint64_t combined = x.low ^ x.high;
  return static_cast<uint8_t>((combined * K64) >> 56);
}

AETHER_VMAPI size_t hash_value(std::string_view str);
AETHER_VMAPI size_t opcode_generator(std::string_view path);

} // namespace aether

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

#if DEBUG || _DEBUG || !NDEBUG
#define AETHER_DEBUG 1
#endif

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

size_t hash_value(std::string_view str);

} // namespace aether

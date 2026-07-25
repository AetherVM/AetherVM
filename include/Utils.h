// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <cstddef>

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

} // namespace aether

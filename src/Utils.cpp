// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Utils.h>
#include <functional>

namespace aether {

size_t hash_value(std::string_view str) {
  std::hash<std::string_view> hasher;
  return hasher(str);
}

std::vector<std::string_view> string_view_split(std::string_view str,
                                                char delimiter) {
  std::vector<std::string_view> result;
  std::size_t start = 0;
  std::size_t end = str.find(delimiter);

  while (end != std::string_view::npos) {
    result.emplace_back(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }

  result.emplace_back(str.substr(start));
  return result;
}

} // namespace aether

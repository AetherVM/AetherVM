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

} // namespace aether

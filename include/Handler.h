// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

#define ISEL_NAME(name) ISEL_##name
#define ISEL_DECL(name) extern const void **ISEL_NAME(name)
#define ISEL_RAW_ITEM(name) {#name, ISEL_NAME(name)}

namespace aether {

struct HandlerRaw {
  const char *name;
  const void **handler;
};

struct Handler {
  size_t hash;
  const void *impl;

  bool operator<(const Handler &rhs) const { return hash < rhs.hash; }
  bool operator==(const Handler &rhs) const { return hash == rhs.hash; }

  static void loadAArch64();
  static void loadX86();

  static std::vector<Handler> aarch64;
  static std::vector<Handler> x86;
};

extern const HandlerRaw HandlerAArch64[];
extern const HandlerRaw HandlerX86[];
extern const size_t HandlerAArch64Num;
extern const size_t HandlerX86Num;

} // namespace aether

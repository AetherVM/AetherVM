// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "IMetaData.h"

#include <string>
#include <string_view>
#include <vector>

namespace aether {

struct RuntimeMetaData {
  std::string_view path;     // path to the original binary
  const IMetaData *metaData; // pointer to the read-only mapped metadata
  uint64_t rtbase, rtend;    // runtime memory range of the original binary
};

struct ChainedInterp {
  std::vector<RuntimeMetaData> runtimes; // list of runtime metadata
  std::string_view cacheRoot;            // path to the cache root directory

  std::string add(std::string_view path, uint64_t rtbase, uint64_t rtend);
  void reset();

  int main(uint64_t entry, int argc, const char *argv[]);
};

extern ChainedInterp chainedInterp;

} // namespace aether

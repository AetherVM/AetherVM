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
  std::string_view path; // path to the original binary

  const IMetaData *metadata; // pointer to the read-only mapped metadata
  size_t mdsize;             // size of the metadata binary

  const IMetaData::InsnMeta *insns; // pointer to the instruction metadata array

  // the next instruction index cache for each instruction
  std::vector<uint32_t> nexticache;

  uint64_t rtbase, rtend; // runtime memory range of the original binary

  RuntimeMetaData() {}
  RuntimeMetaData(std::string_view path, std::string_view aevmpath,
                  uint64_t rtbase, uint64_t rtend);
  ~RuntimeMetaData();

  const IMetaData::InsnMeta *find(uint64_t addr) const;

  auto operator<=>(const RuntimeMetaData &other) const {
    // Compare runtime metadata based on base addresses for ordering
    return rtbase <=> other.rtbase;
  }
};

struct ChainedInterp {
  std::vector<RuntimeMetaData> runtimes; // list of runtime metadata
  std::string_view cacheRoot;            // path to the cache root directory

  std::string add(std::string_view path, uint64_t rtbase, uint64_t rtend);
  void reset();

  const RuntimeMetaData *find(uint64_t addr,
                              const IMetaData::InsnMeta **insn = nullptr) const;

  int main(uint64_t entry, int argc, const char *argv[]);

private:
  void vmEntry(const RuntimeMetaData *rt, const IMetaData::InsnMeta *insn);
};

extern ChainedInterp chainedInterp;

} // namespace aether

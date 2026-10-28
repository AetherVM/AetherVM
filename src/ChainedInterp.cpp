// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "ChainedInterp.h"

#include <Platform.h>
#include <Utils.h>

namespace aether {

ChainedInterp chainedInterp;

RuntimeMetaData::RuntimeMetaData(std::string_view path,
                                 std::string_view aevmpath, uint64_t rtbase,
                                 uint64_t rtend)
    : path(path), rtbase(rtbase), rtend(rtend) {
  metadata =
      reinterpret_cast<const IMetaData *>(map_file_readonly(aevmpath, &mdsize));
  insns = metadata->insns();

  nexticache.resize(metadata->num_instruction);
  // Initialize the next instruction cache with sequential indices
  for (size_t i = 0; i < nexticache.size(); ++i)
    nexticache[i] = i + 1;
}

RuntimeMetaData::~RuntimeMetaData() {
  // Unmap the metadata file
  unmap_file(reinterpret_cast<uintptr_t>(metadata), mdsize);
}

std::string ChainedInterp::add(std::string_view path, uint64_t rtbase,
                               uint64_t rtend) {
  auto aevm =
      (fs::path(cacheRoot) / fs::path(path).filename()).string() + ".aevm";
  if (!fs::exists(aevm))
    return std::format("{} does not exist", aevm);

  runtimes.emplace_back(path, aevm, rtbase, rtend);
  return "";
}

void ChainedInterp::reset() { runtimes.clear(); }

int ChainedInterp::main(uint64_t entry, int argc, const char *argv[]) {
  return -1;
}

} // namespace aether

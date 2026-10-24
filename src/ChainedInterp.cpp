// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "ChainedInterp.h"

namespace aether {

ChainedInterp chainedInterp;

std::string ChainedInterp::add(std::string_view path, uint64_t rtbase,
                               uint64_t rtend) {
  return "Unimplemented";
}

void ChainedInterp::reset() { runtimes.clear(); }

int ChainedInterp::main(uint64_t entry, int argc, const char *argv[]) {
  return -1;
}

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  aether::MachineARM64 arm64;
  // simulate an invalid instruction opcode
  uint64_t opcode_value = 202608041235;
  std::span<const uint8_t> opcode{
      reinterpret_cast<const uint8_t *>(&opcode_value), sizeof(opcode_value)};
  aether::BinaryEngine engine{&arm64};
  if (!engine.execute(opcode)) {
    std::println("Failed to execute.");
    return -1;
  }
  return 0;
}

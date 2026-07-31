// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  aether::MachineARM64 marm64;
  std::string_view asmcode{"mov x0, #1\n"
                           "ret"};
  auto opcode = assemble(&marm64, asmcode);
  if (!opcode.size())
    return -1;

  aether::BinaryEngine engine{&marm64};
  if (!engine.execute(opcode)) {
    std::println("Failed to execute: {}.", asmcode);
    return -1;
  }
  auto x0 = engine.getRegister(aether::Register::X0);
  std::println("X0 = {}", x0->u8);
  return 0;
}

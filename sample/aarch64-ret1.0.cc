// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  aether::MachineARM64 marm64;
  std::string_view asmcode{"fmov d0, #1.0\n"
                           "fmov d1, d0\n"
                           "ret"};
  auto opcode = assemble(&marm64, asmcode);
  if (!opcode.size())
    return -1;

  aether::BinaryEngine engine{&marm64};
  if (!engine.execute(opcode)) {
    std::println("Failed to execute: {}.", asmcode);
    return -1;
  }
  auto q0 = engine.getRegister(aether::Register::Q0);
  auto q1 = engine.getRegister(aether::Register::Q1);
  std::println("Q0.D[0] = {:.1f}, Q1.D[0] = {:.1f}", q0->d, q1->d);
  return 0;
}

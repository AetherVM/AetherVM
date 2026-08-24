// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  aether::MachineX86 mx64;
  std::string_view asmcode{"mov $1, %eax\n"
                           "cvtsi2sd %eax, %xmm0\n"
                           "unpcklpd %xmm0, %xmm0\n"
                           "ret"};
  auto opcode = assemble(&mx64, asmcode);
  if (!opcode.size())
    return -1;

  aether::BinaryEngine engine{&mx64};
  if (!engine.execute(opcode)) {
    std::println("Failed to execute: {}.", asmcode);
    return -1;
  }
  auto xmm0 = engine.getRegister(aether::Register::XMM0);
  std::println("XMM0.D[0] = {:.1f}, XMM0.D[1] = {:.1f}", xmm0[0].d, xmm0[1].d);
  return 0;
}

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  aether::MachineARM64 marm64;
  // 0x2000000 (SYSCALL_CLASS_UNIX) | 0x14 (SYS_getpid)
  std::string_view asmcode{"movz x16, #0x0200, lsl #16\n"
                           "movk x16, #0x0014\n"
                           "svc #0x80\n"
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
  std::println("PID = {}", x0->u8);
  return 0;
}

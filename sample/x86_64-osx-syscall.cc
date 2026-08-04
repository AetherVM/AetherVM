// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  aether::MachineX86 mx64;
  // 0x14 (SYS_getpid)
  std::string_view asmcode{"mov $0x14, %rax\n"
                           "syscall\n"
                           "ret"};
  auto opcode = assemble(&mx64, asmcode);
  if (!opcode.size())
    return -1;

  aether::BinaryEngine engine{&mx64};
  if (!engine.execute(opcode)) {
    std::println("Failed to execute: {}.", asmcode);
    return -1;
  }
  auto rax = engine.getRegister(aether::Register::RAX);
  std::println("PID = {}", rax->u8);
  return 0;
}

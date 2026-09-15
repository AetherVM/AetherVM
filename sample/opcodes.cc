// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"
#include "elf_hash.c"

namespace {

void aarch64_opcodes() {
  aether::MachineARM64 marm64;
  aether::BinaryEngine engine{&marm64};
  engine.prefetch();

  auto insn = "nop";
  auto opcode = assemble(&marm64, insn);
  if (!engine.emulate(opcode))
    std::println("Failed to execute: {}.", insn);

  insn = "mov x0, #1";
  opcode = assemble(&marm64, insn);
  if (engine.emulate(opcode))
    std::println("X0 = {}", engine.getRegister(aether::Register::X0)->u8);
  else
    std::println("Failed to execute: {}.", insn);
}

void x64_opcodes() {
  aether::MachineX86 mx64;
  aether::BinaryEngine engine{&mx64};
  engine.prefetch();

  auto insn = "nop";
  auto opcode = assemble(&mx64, insn);
  if (!engine.emulate(opcode))
    std::println("Failed to execute: {}.", insn);

  insn = "mov $1, %rax";
  opcode = assemble(&mx64, insn);
  if (engine.emulate(opcode))
    std::println("RAX = {}", engine.getRegister(aether::Register::RAX)->u8);
  else
    std::println("Failed to execute: {}.", insn);
}

} // namespace

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  aarch64_opcodes();
  x64_opcodes();
  return 0;
}

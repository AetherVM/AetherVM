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

  engine.emulate(assemble(&marm64, "fmov d0, #1.0"));
#if AETHER_ARCH_ARM64
  insn = "dup v0.2d, v0.d[0]";
#else
  insn = "fmov d1, d0"
#endif
  engine.emulate(assemble(&marm64, insn));
#if AETHER_ARCH_ARM64
  auto q0 = reinterpret_cast<const aether::RegisterValueSIMD *>(
      engine.getRegister(aether::Register::Q0));
  std::println("Q0.D[0] = {:.1f}, Q0.D[1] = {:.1f}", q0->low.d, q0->high.d);
#else
  auto q0 = engine.getRegister(aether::Register::Q0);
  auto q1 = engine.getRegister(aether::Register::Q1);
  std::println("Q0.D[0] = {:.1f}, Q1.D[0] = {:.1f}", q0->d, q1->d);
#endif
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

  engine.emulate(assemble(&mx64, "cvtsi2sd %eax, %xmm0"));
  engine.emulate(assemble(&mx64, "unpcklpd %xmm0, %xmm0"));
  auto xmm0 = reinterpret_cast<const aether::RegisterValueSIMD *>(
      engine.getRegister(aether::Register::XMM0));
  std::println("XMM0.D[0] = {:.1f}, XMM0.D[1] = {:.1f}", xmm0->low.d,
               xmm0->high.d);
}

} // namespace

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  aarch64_opcodes();
  x64_opcodes();
  return 0;
}

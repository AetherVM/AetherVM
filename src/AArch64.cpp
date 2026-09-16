// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

/*
AetherVM AArch64 ABI
────────────────────────

X26     CPU*
X27     Instruction*
X16     next handler

CPU[-0x10]   VM PC*

vm_enter:
    host → VM

handler:
    execute instruction
    X16 = next handler
    jmp X16

event:
    VM → host callback
    callback returns Instruction*
    resume VM
*/

#include "AArch64.h"

#include <charconv>

#if AETHER_ARCH_ARM64

// during the chained execution of the vm handlers:
// x26 is "void *cpu"
// x27 is 'const Instruction *insns'
// [x26-0x10] is pc pointer
// this event handler trampoline does:
// 1.call the real event handler to decide the next instruction to execute;
// 2.construct the right ABI(defined in remill/BC/ABI.h) to run;
#define IMPL_EVENT_VM(n)                                                       \
  AETHER_NAKED void n(void) {                                                  \
    AETHER_ASM("mov x0, x26\n"                                                 \
               "mov x1, #0\n"                                                  \
               "mov x2, x27\n"                                                 \
               "bl " HOST_CALL_PREFIX "host_" #n "\n"                          \
               "mov x27, x0\n"                                                 \
               "mov x0, x26\n"                                                 \
               "ldr x1, [x0, #-0x10]\n"                                        \
               "ldr x1, [x1]\n"                                                \
               "mov x2, x27\n"                                                 \
               "" extract_handler_x16 ""                                       \
               "br x16");                                                      \
  }

IMPL_EVENT_VM(event_func_before);
IMPL_EVENT_VM(event_func_after);
IMPL_EVENT_VM(event_insn_before);
IMPL_EVENT_VM(event_insn_after);
IMPL_EVENT_VM(event_block_before);
IMPL_EVENT_VM(event_block_after);
IMPL_EVENT_VM(event_debugging);
IMPL_EVENT_VM(syscall_interpret);
IMPL_EVENT_VM(interrupt_interpret);
IMPL_EVENT_VM(jump_interpret);
IMPL_EVENT_VM(call_interpret);
IMPL_EVENT_VM(finish_function);
IMPL_EVENT_VM(finish_emulation);
IMPL_EVENT_VM(terminate_execution);
#endif // end of AETHER_ARCH_ARM64

namespace aether {

namespace aarch64 {

#if AETHER_ARCH_ARM64

extern "C" AETHER_NAKED void vm_enter_aarch64(void) {
  AETHER_ASM(
      // save the return address flag to cpu context
      "str lr, [x3]\n"
      // set the return address to vm context
      "str lr, [x4]\n"
      // get the real handler address
      "" extract_handler_x16 ""
      // ABI defined in remill/BC/ABI.h
      // call handler(state, vmaddr, memory)
      "br x16\n");
}

AETHER_VM_ENTRY() {
  AETHER_ASM(
      /*
      Argument ABI:
      x0: void *cpu
      x1: addr_t vmaddr
      x2: const Instruction *insns
      x3: uintptr_t *host_retaddr
      x4: void *(*vm_retaddr)()
      */
      "sub sp, sp, #0x30\n"
      "stp x26, x27, [sp, #0x10]\n"
      "stp fp, lr, [sp, #0x20]\n"
      "add fp, sp, #0x20\n"
      // The extra runtime context of vm handlers:
      // x26: cpu
      // x27: insns
      "mov x26, x0\n"
      "mov x27, x2\n"
      // get vm address storage pointer
      "stp x1, x3, [sp]\n" // save
      "blr x4\n"
      "mov x4, x0\n"
      "mov x0, x26\n"
      "mov x2, x27\n"
      "ldp x1, x3, [sp]\n" // restore
      // do the final initialization and enter vm
      "bl " HOST_CALL_PREFIX "vm_enter_aarch64\n"
      "ldp fp, lr, [sp, #0x20]\n"
      "ldp x26, x27, [sp, #0x10]\n"
      "add sp, sp, #0x30\n"
      "ret");
}

#else

AETHER_VM_ENTRY() { AETHER_ASM("int3"); }

#endif // end of AETHER_ARCH_ARM64

size_t offset_reg(Register reg) {
  // see remill/Arch/AArch64/Runtime/State.h for details
  State *state = nullptr;
  using enum Register;
  static_assert(sizeof(state->simd.v[0]) == 0x10,
                "SIMD register size mismatch");
  if (X0 <= reg && reg <= X31)
    return (size_t)&state->gpr.x0 + 0x10 * ((int)reg - (int)X0);
  if (Q0 <= reg && reg <= Q31)
    return (size_t)&state->simd.v[0] + 0x10 * ((int)reg - (int)Q0);
  abort();
}

size_t offset_reg(std::string_view reg) {
  State *state = nullptr;

  // General Purpose 64-bit (X0 - X30)
  if (reg.starts_with('X')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 30) {
      return (size_t)&state->gpr.x0 + 0x10 * num;
    }
  }

  // General Purpose 32-bit (W0 - W30) - alias to X registers
  if (reg.starts_with('W')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 30) {
      return (size_t)&state->gpr.x0 + 0x10 * num;
    }
  }

  // SIMD / Vector 128-bit (Q0 - Q31 or V0 - V31)
  if (reg.starts_with('Q') || reg.starts_with('V')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 31) {
      return (size_t)&state->simd.v[num];
    }
  }

  // SIMD sub-registers (D0-D31, S0-S31, H0-H31, B0-B31) - alias to SIMD
  // vectors
  if (reg.starts_with('D') || reg.starts_with('S') || reg.starts_with('H') ||
      reg.starts_with('B')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 31) {
      return (size_t)&state->simd.v[num];
    }
  }

  // Special GPRs & Control Registers
  if (reg == "SP" || reg == "WSP")
    return (size_t)&state->gpr.sp;
  if (reg == "PC")
    return (size_t)&state->gpr.pc;

  // Special reused fields within AArch64 state
  if (reg == "XZR" || reg == "WZR")
    return (size_t)&state->padding[0];
  if (reg == "BRANCH_TAKEN")
    return (size_t)&state->gpr._0;
  if (reg == "NEXT_PC")
    return (size_t)&state->gpr._1;
  if (reg == "RETURN_PC")
    return (size_t)&state->gpr._2;
  if (reg == "IGNORE_WRITE_TO_WZR")
    return (size_t)&state->gpr._3;
  if (reg == "IGNORE_WRITE_TO_XZR")
    return (size_t)&state->gpr._4;
  if (reg == "SUPPRESS_WRITEBACK")
    return (size_t)&state->gpr._5;
  if (reg == "MONITOR")
    return (size_t)&state->gpr._6;

  // System Registers (SR)
  if (reg == "TPIDR_EL0")
    return (size_t)&state->sr.tpidr_el0;
  if (reg == "TPIDRRO_EL0")
    return (size_t)&state->sr.tpidrro_el0;

  // Status & Floating-Point Control Registers
  if (reg == "NZCV")
    return (size_t)&state->nzcv;
  if (reg == "FPCR")
    return (size_t)&state->fpcr;
  if (reg == "FPSR")
    return (size_t)&state->fpsr;

  // Individual Condition Flags in SR
  if (reg == "N")
    return (size_t)&state->sr.n;
  if (reg == "Z")
    return (size_t)&state->sr.z;
  if (reg == "C")
    return (size_t)&state->sr.c;
  if (reg == "V")
    return (size_t)&state->sr.v;

  // Individual Cumulative Exception Flags in SR
  if (reg == "IXC")
    return (size_t)&state->sr.ixc;
  if (reg == "OFC")
    return (size_t)&state->sr.ofc;
  if (reg == "UFC")
    return (size_t)&state->sr.ufc;
  if (reg == "IDC")
    return (size_t)&state->sr.idc;
  if (reg == "IOC")
    return (size_t)&state->sr.ioc;
  if (reg == "DZC")
    return (size_t)&state->sr.dzc;

  // Sleigh Flag State
  if (reg == "NG")
    return (size_t)&state->sleigh_flags.NG;
  if (reg == "ZR")
    return (size_t)&state->sleigh_flags.ZR;
  if (reg == "CY")
    return (size_t)&state->sleigh_flags.CY;
  if (reg == "OV")
    return (size_t)&state->sleigh_flags.OV;
  if (reg == "SHIFT_CARRY")
    return (size_t)&state->sleigh_flags.shift_carry;

  abort();
}

} // namespace aarch64

} // namespace aether

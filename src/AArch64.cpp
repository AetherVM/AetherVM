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

#if AETHER_OS_DARWIN_IOS

#define IMPL_OPCODE_CHAIN_STR_LDR(n)                                           \
  AETHER_NAKED void vm_opcode_chain_str_##n(void) {                            \
    AETHER_ASM("str " #n ", [sp, #-0x8]!\n"                                    \
               "" extract_handler_x16 ""                                       \
               "br x16\n");                                                    \
  }                                                                            \
  AETHER_NAKED void vm_opcode_chain_ldr_##n(void) {                            \
    AETHER_ASM("ldr " #n ", [sp], #0x8\n"                                      \
               "" extract_handler_x16 ""                                       \
               "br x16\n");                                                    \
  }

IMPL_OPCODE_CHAIN_STR_LDR(x19);
IMPL_OPCODE_CHAIN_STR_LDR(x20);
IMPL_OPCODE_CHAIN_STR_LDR(x21);
IMPL_OPCODE_CHAIN_STR_LDR(x22);
IMPL_OPCODE_CHAIN_STR_LDR(x23);
IMPL_OPCODE_CHAIN_STR_LDR(x24);
IMPL_OPCODE_CHAIN_STR_LDR(x25);
IMPL_OPCODE_CHAIN_STR_LDR(x26);
IMPL_OPCODE_CHAIN_STR_LDR(x27);
IMPL_OPCODE_CHAIN_STR_LDR(x28);
IMPL_OPCODE_CHAIN_STR_LDR(x29);

IMPL_OPCODE_CHAIN_STR_LDR(d8);
IMPL_OPCODE_CHAIN_STR_LDR(d9);
IMPL_OPCODE_CHAIN_STR_LDR(d10);
IMPL_OPCODE_CHAIN_STR_LDR(d11);
IMPL_OPCODE_CHAIN_STR_LDR(d12);
IMPL_OPCODE_CHAIN_STR_LDR(d13);
IMPL_OPCODE_CHAIN_STR_LDR(d14);
IMPL_OPCODE_CHAIN_STR_LDR(d15);

#undef IMPL_OPCODE_CHAIN_STR_LDR
#define IMPL_OPCODE_CHAIN_STR_LDR(n) (void *)&vm_opcode_chain_str_##n

const void *vm_opcode_chain_str_xs[] = {
    IMPL_OPCODE_CHAIN_STR_LDR(x19), IMPL_OPCODE_CHAIN_STR_LDR(x20),
    IMPL_OPCODE_CHAIN_STR_LDR(x21), IMPL_OPCODE_CHAIN_STR_LDR(x22),
    IMPL_OPCODE_CHAIN_STR_LDR(x23), IMPL_OPCODE_CHAIN_STR_LDR(x24),
    IMPL_OPCODE_CHAIN_STR_LDR(x25), IMPL_OPCODE_CHAIN_STR_LDR(x26),
    IMPL_OPCODE_CHAIN_STR_LDR(x27), IMPL_OPCODE_CHAIN_STR_LDR(x28),
    IMPL_OPCODE_CHAIN_STR_LDR(x29),
};

const void *vm_opcode_chain_str_ds[] = {
    IMPL_OPCODE_CHAIN_STR_LDR(d8),  IMPL_OPCODE_CHAIN_STR_LDR(d9),
    IMPL_OPCODE_CHAIN_STR_LDR(d10), IMPL_OPCODE_CHAIN_STR_LDR(d11),
    IMPL_OPCODE_CHAIN_STR_LDR(d12), IMPL_OPCODE_CHAIN_STR_LDR(d13),
    IMPL_OPCODE_CHAIN_STR_LDR(d14), IMPL_OPCODE_CHAIN_STR_LDR(d15),
};

#undef IMPL_OPCODE_CHAIN_STR_LDR
#define IMPL_OPCODE_CHAIN_STR_LDR(n) (void *)&vm_opcode_chain_ldr_##n

const void *vm_opcode_chain_ldr_xs[] = {
    IMPL_OPCODE_CHAIN_STR_LDR(x19), IMPL_OPCODE_CHAIN_STR_LDR(x20),
    IMPL_OPCODE_CHAIN_STR_LDR(x21), IMPL_OPCODE_CHAIN_STR_LDR(x22),
    IMPL_OPCODE_CHAIN_STR_LDR(x23), IMPL_OPCODE_CHAIN_STR_LDR(x24),
    IMPL_OPCODE_CHAIN_STR_LDR(x25), IMPL_OPCODE_CHAIN_STR_LDR(x26),
    IMPL_OPCODE_CHAIN_STR_LDR(x27), IMPL_OPCODE_CHAIN_STR_LDR(x28),
    IMPL_OPCODE_CHAIN_STR_LDR(x29),
};

const void *vm_opcode_chain_ldr_ds[] = {
    IMPL_OPCODE_CHAIN_STR_LDR(d8),  IMPL_OPCODE_CHAIN_STR_LDR(d9),
    IMPL_OPCODE_CHAIN_STR_LDR(d10), IMPL_OPCODE_CHAIN_STR_LDR(d11),
    IMPL_OPCODE_CHAIN_STR_LDR(d12), IMPL_OPCODE_CHAIN_STR_LDR(d13),
    IMPL_OPCODE_CHAIN_STR_LDR(d14), IMPL_OPCODE_CHAIN_STR_LDR(d15),
};

#define IMPL_OPCODE_CHAIN_SAVE_X26(n)                                          \
  AETHER_NAKED void vm_opcode_chain_save_x26_##n(void) {                       \
    AETHER_ASM("mov " #n ", x26\n"                                             \
               "" extract_handler_x16 ""                                       \
               "br x16\n");                                                    \
  }

IMPL_OPCODE_CHAIN_SAVE_X26(x0);
IMPL_OPCODE_CHAIN_SAVE_X26(x1);
IMPL_OPCODE_CHAIN_SAVE_X26(x2);
IMPL_OPCODE_CHAIN_SAVE_X26(x3);
IMPL_OPCODE_CHAIN_SAVE_X26(x4);
IMPL_OPCODE_CHAIN_SAVE_X26(x5);

#undef IMPL_OPCODE_CHAIN_SAVE_X26
#define IMPL_OPCODE_CHAIN_SAVE_X26(n) (void *)&vm_opcode_chain_save_x26_##n

const void *vm_opcode_chain_save_x26[] = {
    IMPL_OPCODE_CHAIN_SAVE_X26(x0), IMPL_OPCODE_CHAIN_SAVE_X26(x1),
    IMPL_OPCODE_CHAIN_SAVE_X26(x2), IMPL_OPCODE_CHAIN_SAVE_X26(x3),
    IMPL_OPCODE_CHAIN_SAVE_X26(x4), IMPL_OPCODE_CHAIN_SAVE_X26(x5),
};

#endif // end of  AETHER_OS_DARWIN_IOS

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

  // General Purpose 64-bit (x0 - x30)
  if (reg.starts_with('x')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 30) {
      return (size_t)&state->gpr.x0 + 0x10 * num;
    }
  }

  // General Purpose 32-bit (w0 - w30) - alias to X registers
  if (reg.starts_with('w')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 30) {
      return (size_t)&state->gpr.x0 + 0x10 * num;
    }
  }

  // SIMD / Vector 128-bit (q0 - q31 or v0 - v31)
  if (reg.starts_with('q') || reg.starts_with('v')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 31) {
      return (size_t)&state->simd.v[num];
    }
  }

  // SIMD sub-registers (d0-d31, s0-s31, h0-h31, b0-b31) - alias to SIMD
  // vectors
  if (reg.starts_with('d') || reg.starts_with('s') || reg.starts_with('h') ||
      reg.starts_with('b')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 31) {
      return (size_t)&state->simd.v[num];
    }
  }

  // Special GPRs & Control Registers
  if (reg == "sp" || reg == "wsp")
    return (size_t)&state->gpr.sp;
  if (reg == "pc")
    return (size_t)&state->gpr.pc;
  if (reg == "xzr" || reg == "wzr")
    return 0; // Zero register read-only offset

  // System Registers (SR)
  if (reg == "tpidr_el0")
    return (size_t)&state->sr.tpidr_el0;
  if (reg == "tpidrro_el0")
    return (size_t)&state->sr.tpidrro_el0;

  // Status & Floating-Point Control Registers
  if (reg == "nzcv")
    return (size_t)&state->nzcv;
  if (reg == "fpcr")
    return (size_t)&state->fpcr;
  if (reg == "fpsr")
    return (size_t)&state->fpsr;

  // Individual Condition Flags in SR
  if (reg == "n")
    return (size_t)&state->sr.n;
  if (reg == "z")
    return (size_t)&state->sr.z;
  if (reg == "c")
    return (size_t)&state->sr.c;
  if (reg == "v")
    return (size_t)&state->sr.v;

  // Individual Cumulative Exception Flags in SR
  if (reg == "ixc")
    return (size_t)&state->sr.ixc;
  if (reg == "ofc")
    return (size_t)&state->sr.ofc;
  if (reg == "ufc")
    return (size_t)&state->sr.ufc;
  if (reg == "idc")
    return (size_t)&state->sr.idc;
  if (reg == "ioc")
    return (size_t)&state->sr.ioc;
  if (reg == "dzc")
    return (size_t)&state->sr.dzc;

  // Sleigh Flag State
  if (reg == "ng")
    return (size_t)&state->sleigh_flags.NG;
  if (reg == "zr")
    return (size_t)&state->sleigh_flags.ZR;
  if (reg == "cy")
    return (size_t)&state->sleigh_flags.CY;
  if (reg == "ov")
    return (size_t)&state->sleigh_flags.OV;
  if (reg == "shift_carry")
    return (size_t)&state->sleigh_flags.shift_carry;

  abort();
}

} // namespace aarch64

} // namespace aether

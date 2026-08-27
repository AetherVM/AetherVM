// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "X86.h"
#include <Platform.h>

#if AETHER_ARCH_X64

#if AETHER_OS_DARWIN
#define HOST_CALL_PREFIX "_"
#else
#define HOST_CALL_PREFIX ""
#endif

// r13 is 'const Instruction *insns'
#if AETHER_OS_WINDOWS
#define ARGREG_0 "rcx"
#define ARGREG_1 "rdx"
#define ARGREG_2 "r8"
#define ARGREG_3 "r9"
#else
#define ARGREG_0 "rdi"
#define ARGREG_1 "rsi"
#define ARGREG_2 "rdx"
#define ARGREG_3 "rcx"
#endif

// r13 is 'const Instruction *insns'
#define IMPL_EVENT_VM(n)                                                       \
  AETHER_NAKED void n(void) {                                                  \
    AETHER_ASM("mov %r13, " ARGREG_0 "\n"                                      \
               "call " HOST_CALL_PREFIX "host_" #n "\n"                        \
               "mov %rax, %r13\n"                                              \
               "jmp *%r13");                                                   \
  }

IMPL_EVENT_VM(event_func_before);
IMPL_EVENT_VM(event_func_after);
IMPL_EVENT_VM(event_insn_before);
IMPL_EVENT_VM(event_insn_after);
IMPL_EVENT_VM(event_syscall_before);
IMPL_EVENT_VM(event_syscall_after);
IMPL_EVENT_VM(event_trap_before);
IMPL_EVENT_VM(event_trap_after);
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
#endif // end of AETHER_ARCH_X64

namespace aether {

namespace x86 {

#if AETHER_ARCH_X64

AETHER_VM_ENTRY() {
  /*
  r12: cpu
  r13: insns
  */
  AETHER_ASM("int3");
}

#else

AETHER_VM_ENTRY() { AETHER_ASM("brk #0"); }

#endif // end of AETHER_ARCH_X64

size_t offset_reg(Register reg) {
  // see remill/Arch/X86/Runtime/State.h for details
  State *state = nullptr;
  using enum Register;
  switch (reg) {
  case RIP:
    return (size_t)&state->gpr.rip;
  case RAX:
    return (size_t)&state->gpr.rax;
  case RBP:
    return (size_t)&state->gpr.rbp;
  case RBX:
    return (size_t)&state->gpr.rbx;
  case RCX:
    return (size_t)&state->gpr.rcx;
  case RDI:
    return (size_t)&state->gpr.rdi;
  case RDX:
    return (size_t)&state->gpr.rdx;
  case RSI:
    return (size_t)&state->gpr.rsi;
  case RSP:
    return (size_t)&state->gpr.rsp;
  case R8:
    return (size_t)&state->gpr.r8;
  case R9:
    return (size_t)&state->gpr.r9;
  case R10:
    return (size_t)&state->gpr.r10;
  case R11:
    return (size_t)&state->gpr.r11;
  case R12:
    return (size_t)&state->gpr.r12;
  case R13:
    return (size_t)&state->gpr.r13;
  case R14:
    return (size_t)&state->gpr.r14;
  case R15:
    return (size_t)&state->gpr.r15;
  default:
    break;
  }
  if (ST0 <= reg && reg <= ST7)
    return (size_t)&state->st.elems[0] + 0xA * ((int)reg - (int)ST0);
  if (MM0 <= reg && reg <= MM7)
    return (size_t)&state->mmx.elems[0] + 0x8 * ((int)reg - (int)MM0);
  if (XMM0 <= reg && reg <= XMM31)
    return (size_t)&state->vec[0].xmm + 0x40 * ((int)reg - (int)XMM0);
  abort();
}

} // namespace x86

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "X86.h"
#include <Platform.h>

#if AETHER_ARCH_X64

// during the chained execution of the vm handlers:
// r12 is "void *cpu"
// r13 is 'const Instruction *insns'
// [r12-0x10] is pc pointer
// this event handler trampoline does:
// 1.call the real event handler to decide the next instruction to execute;
// 2.construct the right ABI(defined in remill/BC/ABI.h) to run;
#define IMPL_EVENT_VM_IMPL(n)                                                  \
  AETHER_ASM("mov %r12, %" ARGREG_0 "\n"                                       \
             "mov $0, %" ARGREG_1 "\n"                                         \
             "mov %r13, %" ARGREG_2 "\n"                                       \
             "call " HOST_CALL_PREFIX "host_" n "\n"                           \
             "mov %rax, %r13\n"                                                \
             "mov %r12, %" ARGREG_0 "\n"                                       \
             "mov -0x10(%r12), %" ARGREG_1 "\n"                                \
             "mov 0x0(%" ARGREG_1 "), %" ARGREG_1 "\n"                         \
             "mov %r13, %" ARGREG_2 "\n"                                       \
             "" extract_handler_r10 "")

#if AETHER_OS_WINDOWS
#define IMPL_EVENT_VM(n)                                                       \
  AETHER_NAKED void n(void) {                                                  \
    /* 0x20 shadow space + 0x08 alignment */                                   \
    AETHER_ASM("sub $0x28, %rsp");                                             \
    IMPL_EVENT_VM_IMPL(#n);                                                    \
    AETHER_ASM("add $0x28, %rsp\n"                                             \
               "jmp *%r10");                                                   \
  }
#else
#define IMPL_EVENT_VM(n)                                                       \
  AETHER_NAKED void n(void) {                                                  \
    IMPL_EVENT_VM_IMPL(#n);                                                    \
    AETHER_ASM("jmp *%r10");                                                   \
  }
#endif

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

extern "C" AETHER_NAKED void vm_enter_x64(void) {
  AETHER_ASM(
      // get the return address
      "mov 0x0(%rsp), %r10\n"
      // save the return address flag to cpu context
      "mov %r10, 0x0(%" ARGREG_3 ")\n"
      // set the return address to vm context
      "mov %r10, 0x0(%rax)\n"
      // get the real handler address
      "" extract_handler_r10 ""
      // ABI defined in remill/BC/ABI.h
      // call handler(state, vmaddr, memory)
      "jmp *%r10\n");
}

AETHER_VM_ENTRY() {
  AETHER_ASM(
#if AETHER_OS_WINDOWS
      /*
      On Windows x64, every caller must allocate 32 bytes (0x20) of stack
      space before making a function call, regardless of how many parameters
      are passed. This space is reserved directly at the top of the stack
      (rsp + 0x00 through rsp + 0x1F). Plus another 8 bytes for the return
      address, so the fifth argument is passed on the stack at [rsp + 0x20 +
      0x8].
      */
      "mov 0x28(%rsp), %rax\n"
#else
      "mov %" ARGREG_4 ", %rax\n"
#endif
      /*
      Argument ABI:
      REGARG_0: void *cpu
      REGARG_1: addr_t vmaddr
      REGARG_2: const Instruction *insns
      REGARG_3: uintptr_t *host_retaddr
      rax: void *(*vm_retaddr)()
      */
      "push %r12\n"
      "push %r13\n"
      // The extra runtime context of vm handlers:
      // r12: cpu
      // r13: insns
      "mov %" ARGREG_0 ", %r12\n"
      "mov %" ARGREG_2 ", %r13\n"
      "push %" ARGREG_1 "\n" // save vmaddr
      "push %" ARGREG_3 "\n" // save host_retaddr
#if AETHER_OS_WINDOWS
      // shadow space (0x20) + alignment padding (0x08)
      "sub $0x28, %rsp\n"
#endif
      "call *%rax\n"
#if AETHER_OS_WINDOWS
      "add $0x28, %rsp\n"
#endif
      "pop %" ARGREG_3 "\n" // restore host_retaddr
      "pop %" ARGREG_1 "\n" // restore vmaddr
      "mov %r12, %" ARGREG_0 "\n"
      "mov %r13, %" ARGREG_2 "\n"
#if AETHER_OS_WINDOWS
      "sub $0x28, %rsp\n"
#endif
      "call " HOST_CALL_PREFIX "vm_enter_x64\n"
#if AETHER_OS_WINDOWS
      // shadow space + alignment padding + return address
      "add $0x30, %rsp\n"
#else
      "add $0x8, %rsp\n" // pop return address
#endif
      "pop %r13\n"
      "pop %r12\n"
      "ret");
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

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
#include <Platform.h>

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
  if (X0 <= reg && reg <= X31)
    return (size_t)&state->gpr.x0 + 0x10 * ((int)reg - (int)X0);
  if (Q0 <= reg && reg <= Q31)
    return (size_t)&state->simd.v[0] + 0x10 * ((int)reg - (int)Q0);
  abort();
}

} // namespace aarch64

} // namespace aether

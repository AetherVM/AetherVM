// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "AArch64.h"
#include <Platform.h>

#if AETHER_ARCH_ARM64

#if AETHER_OS_MACOS
#define HOST_CALL_PREFIX "_host_"
#else
#define HOST_CALL_PREFIX "host_"
#endif

// during the chained execution of the vm handlers:
// x26 is "void *cpu"
// x27 is 'const Instruction *insns'
// [x26] is pc pointer
// this event handler trampoline does:
// 1.call the real event handler to decide the next instruction to execute;
// 2.construct the right ABI(defined in remill/BC/ABI.h) to run;
#define IMPL_EVENT_VM(n)                                                       \
  AETHER_NAKED void n(void) {                                                  \
    AETHER_ASM("mov x0, x26\n"                                                 \
               "mov x1, #0\n"                                                  \
               "mov x2, x27\n"                                                 \
               "bl " HOST_CALL_PREFIX #n "\n"                                  \
               "mov x16, x0\n"                                                 \
               "mov x0, x26\n"                                                 \
               "ldr x1, [x0]\n"                                                \
               "ldr x1, [x1]\n"                                                \
               "mov x2, x27\n"                                                 \
               "br x16");                                                      \
  }

IMPL_EVENT_VM(event_func_before);
IMPL_EVENT_VM(event_func_after);
IMPL_EVENT_VM(event_insn_before);
IMPL_EVENT_VM(event_insn_after);
IMPL_EVENT_VM(event_block_before);
IMPL_EVENT_VM(event_block_after);
IMPL_EVENT_VM(jump_interpret);
IMPL_EVENT_VM(call_interpret);
IMPL_EVENT_VM(finish_function);
IMPL_EVENT_VM(terminate_execution);
#endif // end of AETHER_ARCH_ARM64

namespace aether {

namespace aarch64 {

#if AETHER_ARCH_ARM64

AETHER_VM_ENTRY() {
  AETHER_ASM(
      /*
      Argument ABI:
      x0: void *cpu
      x1: addr_t vmaddr
      x2: const Instruction *insns
      x3: uintptr_t *retaddr
      x4: void (*init_stack_pointer)(uintptr_t retaddr)
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
      // save the return address flag to cpu context
      "str lr, [x3]\n"
      // init vm stack pointer
      "str x1, [sp]\n" // save x1
      "mov x0, lr\n"
      "blr x4\n"
      "ldr x1, [sp]\n" // restore x1
      // get the real handler address
      "ldr x16, [x27]\n"
      "ubfx x16, x16, #0, #59\n"
      // ABI defined in remill/BC/ABI.h
      // call handler(state, vmaddr, memory)
      "blr x16\n"
      "ldp fp, lr, [sp, #0x20]\n"
      "ldp x26, x27, [sp, #0x10]\n"
      "add sp, sp, #0x30\n"
      "ret");
}

#else

AETHER_VM_ENTRY() { AETHER_ASM("int3"); }

#endif // end of AETHER_ARCH_ARM64

} // namespace aarch64

} // namespace aether

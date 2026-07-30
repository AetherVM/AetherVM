// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "X86.h"
#include <Platform.h>

#if AETHER_ARCH_X64

#if AETHER_OS_MACOS
#define HOST_CALL_PREFIX "_host_"
#else
#define HOST_CALL_PREFIX "host_"
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
               "call " HOST_CALL_PREFIX #n "\n"                                \
               "mov %rax, %r13\n"                                              \
               "jmp *%r13");                                                   \
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
#endif // end of AETHER_ARCH_X64

namespace aether {

namespace x86 {

#if AETHER_ARCH_X64

AETHER_NAKED void host_vm_entry(void *cpu, addr_t vmaddr,
                                const Instruction *insns, uintptr_t *retaddr) {
  /*
  r12: cpu
  r13: insns
  */
  AETHER_ASM("");
}

#else

AETHER_NAKED void host_vm_entry(void *cpu, addr_t vmaddr,
                                const Instruction *insns, uintptr_t *retaddr) {
  AETHER_ASM("brk #0");
}

#endif // end of AETHER_ARCH_X64

} // namespace x86

} // namespace aether

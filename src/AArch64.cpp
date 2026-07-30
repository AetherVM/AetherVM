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

// x27 is 'const Instruction *insns'
#define IMPL_EVENT_VM(n)                                                       \
  AETHER_NAKED void n(void) {                                                  \
    AETHER_ASM("mov x0, x27\n"                                                 \
               "bl " HOST_CALL_PREFIX #n "\n"                                  \
               "mov x16, x0\n"                                                 \
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

AETHER_NAKED void host_vm_entry(void *cpu, addr_t vmaddr,
                                const Instruction *insns) {
  /*
  x26: cpu
  x27: insns
  */
  AETHER_ASM("");
}

#else

AETHER_NAKED void host_vm_entry(void *cpu, addr_t vmaddr,
                                const Instruction *insns) {
  AETHER_ASM("int 3");
}

#endif // end of AETHER_ARCH_ARM64

} // namespace aarch64

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "BinaryEngine.h"
#include "Orchestrator.h"
#include <Platform.h>

#if AETHER_OS_DARWIN
extern "C" int syscall(int number, ...);
#else
#endif

IMPL_EVENT_HOST(event_func_before) { return forward_event_default(); }

IMPL_EVENT_HOST(event_func_after) { return forward_event_default(); }

IMPL_EVENT_HOST(event_insn_before) { return forward_event_default(); }

IMPL_EVENT_HOST(event_insn_after) { return forward_event_default(); }

IMPL_EVENT_HOST(event_block_before) { return forward_event_default(); }

IMPL_EVENT_HOST(event_block_after) { return forward_event_default(); }

IMPL_EVENT_HOST(event_syscall_before) { return forward_event_default(); }

IMPL_EVENT_HOST(event_syscall_after) { return forward_event_default(); }

IMPL_EVENT_HOST(event_trap_before) { return forward_event_default(); }

IMPL_EVENT_HOST(event_trap_after) { return forward_event_default(); }

IMPL_EVENT_HOST(event_debugging) {
  decl_cpu();
  cpu->runtime->dbgContext.insn_handler(state, vmaddr, current);
  return forward_event_default();
}

IMPL_EVENT_HOST(syscall_interpret) {
  // only arm64 guest will use this event handler, x86_64 guest will use
  // __remill_sync_hyper_call
  decl_cpu();
  auto gpr = &cpu->aarch64.gpr;
#if AETHER_OS_DARWIN
  gpr->x0.qword = syscall(gpr->x0.qword, gpr->x1, gpr->x2, gpr->x3, gpr->x4,
                          gpr->x5, gpr->x6, gpr->x7);
#else
#error TODO:: implement syscall_interpret for non-macOS platforms
#endif
  return forward_event_default();
}

IMPL_EVENT_HOST(interrupt_interpret) {
  // only x86_64 guest will use this event handler
  decl_cpu();
  auto gpr = &cpu->x86.gpr;
  printf("[AetherVM] X86_64 guest hit an interrupt instruction before 0x%llx\n",
         gpr->rip.qword);
  abort();
  return forward_event_default();
}

IMPL_EVENT_HOST(jump_interpret) { abort(); }

IMPL_EVENT_HOST(call_interpret) { abort(); }

IMPL_EVENT_HOST(finish_function) {
  decl_cpu();
  return cpu->pcptr[0] == cpu->retaddr ? forward_event(finish_emulation)
                                       : forward_event(jump_interpret);
}

IMPL_EVENT_HOST(finish_emulation) {
  decl_cpu();
  return reinterpret_cast<aether::Instruction *>(&cpu->retaddr);
}

// run to an invalid vm address
IMPL_EVENT_HOST(terminate_execution) { abort(); };

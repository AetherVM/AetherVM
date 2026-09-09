// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "BinaryEngine.h"
#include "Orchestrator.h"
#include <Platform.h>

#if AETHER_OS_DARWIN || AETHER_OS_LINUX
extern "C" int syscall(int number, ...);
#else
#endif

using namespace aether;

IMPL_EVENT_HOST(event_func_before) {
  decl_cpu();
  auto engine = cpu->runtime;
  if (engine->hasEventHandler()) {
    Event event = EventRuntime{EventType::FuncBefore, *cpu->pcptr};
    engine->handleEvent(event);
  }
  return forward_event_default();
}

IMPL_EVENT_HOST(event_func_after) {
  decl_cpu();
  auto engine = cpu->runtime;
  if (engine->hasEventHandler()) {
    Event event = EventRuntime{EventType::FuncAfter, *cpu->pcptr};
    engine->handleEvent(event);
  }
  return forward_event_default();
}

IMPL_EVENT_HOST(event_insn_before) {
  decl_cpu();
  auto engine = cpu->runtime;
  if (engine->hasEventHandler()) {
    Event event = EventRuntime{EventType::InsnBefore, *cpu->pcptr};
    switch (engine->handleEvent(event)) {
    case EventResult::Processed:
      // skip the real handler
      return forward_event_default() + 1;
    case EventResult::Terminate:
      // user told us to finish the current emulation
      return forward_event(finish_emulation);
    default:
      break;
    }
  }
  return forward_event_default();
}

IMPL_EVENT_HOST(event_insn_after) {
  decl_cpu();
  auto engine = cpu->runtime;
  if (engine->hasEventHandler()) {
    Event event = EventRuntime{EventType::InsnAfter, *cpu->pcptr};
    engine->handleEvent(event);
  }
  return forward_event_default();
}

IMPL_EVENT_HOST(event_block_before) {
  decl_cpu();
  auto engine = cpu->runtime;
  if (engine->hasEventHandler()) {
    Event event = EventRuntime{EventType::BlockBefore, *cpu->pcptr};
    engine->handleEvent(event);
  }
  return forward_event_default();
}

IMPL_EVENT_HOST(event_block_after) {
  decl_cpu();
  auto engine = cpu->runtime;
  if (engine->hasEventHandler()) {
    Event event = EventRuntime{EventType::BlockAfter, *cpu->pcptr};
    engine->handleEvent(event);
  }
  return forward_event_default();
}

IMPL_EVENT_HOST(event_debugging) {
  decl_cpu();
  if (cpu->runtime->eventConf.debug)
    cpu->runtime->dbgContext.insn_handler(state, cpu->pcptr[0], current);
  return forward_event_default();
}

IMPL_EVENT_HOST(syscall_interpret) {
  // only arm64 guest will use this event handler, x86_64 guest will use
  // __remill_sync_hyper_call
  decl_cpu();
  auto engine = cpu->runtime;
  auto gpr = &cpu->aarch64.gpr;
#if AETHER_OS_DARWIN || AETHER_OS_LINUX
  Event event = EventHyperCall{{EventType::SyscallBefore, *cpu->pcptr},
                               {.sysno = (int)gpr->x0.qword}};
  if (engine->handleEvent(event) != EventResult::Processed) {
    gpr->x0.qword = syscall(gpr->x0.qword, gpr->x1, gpr->x2, gpr->x3, gpr->x4,
                            gpr->x5, gpr->x6, gpr->x7);
    if (engine->hasEventHandler()) {
      std::get<EventHyperCall>(event).type = EventType::SyscallAfter;
      engine->handleEvent(event);
    }
  }
#else
  printf("[AetherVM]  TODO:: implement syscall_interpret for non-POSIX "
         "platforms.\n");
  abort();
#endif
  return forward_event_default();
}

IMPL_EVENT_HOST(interrupt_interpret) {
  // only x86_64 guest will use this event handler
  decl_cpu();
  auto engine = cpu->runtime;
  Event event = EventRuntime{EventType::TrapHit, *cpu->pcptr};
  engine->handleEvent(event);

  auto gpr = &cpu->x86.gpr;
  printf("[AetherVM] X86_64 guest hit an interrupt instruction before 0x%llx\n",
         gpr->rip.qword);
  abort();
  return forward_event_default();
}

IMPL_EVENT_HOST(jump_interpret) {
  decl_cpu();
  return aether::Orchestrator::inst()->find(cpu->pcptr[0]);
}

IMPL_EVENT_HOST(call_interpret) {
  decl_cpu();
  return aether::Orchestrator::inst()->find(cpu->pcptr[0]);
}

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

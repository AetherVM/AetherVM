// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <functional>
#include <variant>

namespace aether {

enum class EventType {
  // Remill Lifting Operations
  LiftBefore,
  LiftAfter,

  // Memory Operations & Structural Changes
  MemRead,
  MemWrite,
  MemMap,
  MemUnmap,
  MemProtect,

  // Execution Flow
  InsnBefore,
  InsnAfter,
  BlockBefore,
  BlockAfter,

  // Specialized Instruction Boundaries
  // syscall
  SyscallBefore,
  SyscallAfter,
  // breakpoint
  TrapBefore,
  TrapAfter,
  // native bridge
  HostBridgeBefore,
  HostBridgeAfter,

  // Exceptional States
  ExceptionThrown,
  InvalidInsn,
};

enum class EventResult {
  // Continue the current procedure.
  Continue,
  // Skip the current procedure as the result indicates it processed.
  Processed,
  // Terminate the current task.
  Terminate,
};

struct EventRuntime {
  EventType type; // the runtime operation type
  uint64_t addr;  // the vm address where this event happen
};

struct EventLift : public EventRuntime {
  uint8_t opcode[16]; // the opcode of this address
  // if LiftBefore it's empty
  // if LiftAfter it's the handler name
  std::string_view name;
};

struct EventMemory : public EventRuntime {
  size_t size;
};

struct EventHyperCall : public EventRuntime {
  union {
    int sysno;       // if Syscall it's syscall number
    uint64_t target; // if Host it's host native target address
  };
};

using Event =
    std::variant<EventRuntime, EventLift, EventMemory, EventHyperCall>;
using EventCallback = std::function<EventResult(Event &)>;

} // namespace aether

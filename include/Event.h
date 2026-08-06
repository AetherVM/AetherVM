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
  FuncBefore,
  FuncAfter,

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

struct EventConfig {
  // lift event
  uint64_t lift : 1 = false;
  // memory operation event
  uint64_t memory : 1 = false;
  // function event
  uint64_t func : 1 = false;
  // basic block event
  uint64_t block : 1 = false;
  // instruction/syscall/trap event
  uint64_t insn : 1 = false;
  uint64_t syscall : 1 = false;
  uint64_t trap : 1 = false;
  // vm-host bridge event
  uint64_t bridge : 1 = false;
  // debugger
  uint64_t debug : 1 = false;

  uint64_t reserved : 55 = 0;
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

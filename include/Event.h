// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <functional>

namespace aether {

enum class EventType {
  // Remill Lifting Operations
  LiftBefore,
  LiftAfter,
  LiftError,

  // Memory Operations & Structural Changes
  MemRead,
  MemWrite,
  MemFetch,
  MemMap,
  MemUnmap,
  MemProtect,

  // Execution Flow
  InsnBefore,
  InsnAfter,
  BlockBefore,
  BlockAfter,

  // Specialized Instruction Boundaries
  SyscallBefore,
  SyscallAfter,
  TrapBefore,
  TrapAfter,
  HostBefore,
  HostAfter,

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

struct Event {
  EventType type;

  union {
    addr_t address;
  };
};

using EventCallback = std::function<EventResult(Event &)>;

} // namespace aether

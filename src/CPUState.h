// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

// must include before AArch64.h and X86.h to make remill definition available
// for both of them
#include <remill/Arch/Runtime/State.h>
#include <remill/Arch/Runtime/Types.h>
#include <remill/BC/Util.h>
#include <remill/OS/OS.h>

#include "AArch64.h"
#include "X86.h"
#include <Register.h>

namespace aether {

struct BinaryEngineImpl;

struct CPUState {
  // 1MB stack size for each thread
  static constexpr size_t stackSize = 1 * 1024 * 1024;

  BinaryEngineImpl *runtime = nullptr;
  char *stack = nullptr;
  union {
    aarch64::State aarch64;
    x86::State x86;
  };

  CPUState() {}
  ~CPUState() {}

  bool initContext(addr_t entry);

  // should be explicitly called after child thread has exited
  void freeContext();

  const RegisterValue *getRegisterAArch64(Register reg);
  bool setRegisterAArch64(Register reg, RegisterValue val);

  const RegisterValue *getRegisterX86(Register reg);
  bool setRegisterX86(Register reg, RegisterValue val);
};

// execution state for each thread
extern thread_local CPUState CPU;

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <AetherVM.h>

struct AetherDbgContext {
  // input from AetherVM
  aether::BinaryEngine *engine; // the current binary engine instance
  int port;                     // debug server listing port
  bool arm64;                   // if arm64 otherwise x86_64

  // output from AetherDbg
  void (*thread_handler)(uintptr_t *pcptr);
  void (*insn_handler)(void *state, uintptr_t pc, const void *insn);
};

using aether_dbgmain_t = void (*)(AetherDbgContext *);

extern "C" AETHER_VMAPI void aether_dbgmain(AetherDbgContext *context);

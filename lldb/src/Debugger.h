// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <stdint.h>

#ifdef _WIN32
#ifdef AETHER_DLLIMPL
#define __AETHER_DBGAPI__ __declspec(dllexport)
#else
#define __AETHER_DBGAPI__ __declspec(dllimport)
#endif // end of AETHER_DLLIMPL
#else
#define __AETHER_DBGAPI__ __attribute__((visibility("default")))
#endif // end of _WIN32

class BinaryEngine;

struct AetherDbgContext {
  // input from AetherVM
  BinaryEngine *engine; // the current binary engine instance
  uintptr_t *pcptr; // the pointer to the current pc in the guest state

  // output from AetherDbg
  void (*insn_handler)(void);
};

extern "C" __AETHER_DBGAPI__ void aether_dbgmain(AetherDbgContext *context);

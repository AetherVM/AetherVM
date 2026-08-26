// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "CPUState.h"
#include "Lifter.h"
#include "Memory.h"

#include <AetherArch.h>
#include <Debugger.h>
#include <Event.h>

#include <mutex>

namespace aether {

struct BinaryEngineImpl {
  EventConfig eventConf;
  ArchType arch;
  GuestMemory guestMemory;
  addr_t vmBase = 0;

  std::mutex mutex;
  std::vector<EventCallback> eventCallbacks;

  llvm::LLVMContext llvmContext;
  remill::Arch::ArchPtr remillArch;
  std::unique_ptr<llvm::Module> remillSemantic;

  AetherDbgContext dbgContext;

  BinaryEngineImpl(ArchType arch, FileType os, EventConfig cfg,
                   BinaryEngine *engine);
  ~BinaryEngineImpl();

  addr_t guest2Host(addr_t target) {
    return guestMemory.valid(target, 1) ? guestMemory.host(target) : target;
  }

  bool startVM(addr_t entry);

private:
  void startDebugger(ArchType type);
};

} // namespace aether

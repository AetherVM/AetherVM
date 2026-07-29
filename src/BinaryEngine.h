// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "CPUState.h"
#include "Lifter.h"
#include "Memory.h"

#include <AetherArch.h>
#include <Event.h>

#include <mutex>

namespace aether {

struct BinaryEngineImpl {
  ArchType arch;
  GuestMemory guestMemory;

  std::mutex mutex;
  std::vector<EventCallback> eventCallbacks;

  llvm::LLVMContext llvmContext;
  remill::Arch::ArchPtr remillArch;
  std::unique_ptr<llvm::Module> remillSemantic;

  BinaryEngineImpl(ArchType arch, FileType os);
  ~BinaryEngineImpl();

  bool startVM(addr_t entry);
};

} // namespace aether

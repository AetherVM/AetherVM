// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "CPUState.h"
#include "Lifter.h"
#include "Memory.h"
#include "OpcodeEngine.h"

#include <AetherArch.h>
#include <Debugger.h>
#include <Disassembler.h>
#include <Event.h>

#include <mutex>
#include <unordered_map>

namespace aether {

struct BinaryEngineImpl {
  EventConfig eventConf;
  ArchType arch;
  FileType osFile;
  GuestMemory guestMemory;

  std::mutex mutex;
  std::vector<EventCallback> eventCallbacks;

  llvm::LLVMContext llvmContext;
  remill::Arch::ArchPtr remillArch;
  std::unique_ptr<llvm::Module> remillSemantic;

  AetherDbgContext dbgContext;
  OpcodeEngine opcodeEmu;
  Disassembler diser;

  // Maximum number of stubs that can be generated, see
  // tools/wrapper_entry_generator.cc for details.
  constexpr static int kMaxStubs = 100000;
  int stubCount = 0;
  std::map<uint64_t, const uint8_t *> stubMap; // vm address -> stub pointer
  std::unordered_map<const uint8_t *, uint64_t>
      stubTargets; // stub -> vm address

  BinaryEngineImpl(ArchType arch, FileType os, EventConfig cfg,
                   BinaryEngine *engine);
  ~BinaryEngineImpl();

  bool startVM(addr_t entry);

  bool hasEventHandler() const { return eventCallbacks.size() > 0; }

  EventResult handleEvent(Event &event) {
    for (auto &callback : eventCallbacks) {
      auto result = callback(event);
      if (result != EventResult::Continue)
        return result;
    }
    return EventResult::Continue;
  }

  template <typename T> bool emulate(T opcode) {
    return opcodeEmu.emulate(opcode);
  }

  const void *makeExecutable(addr_t target);

private:
  void startDebugger(ArchType type);
};

} // namespace aether

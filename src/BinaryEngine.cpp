// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "BinaryEngine.h"
#include "Orchestrator.h"
#include <Platform.h>
#include <Utils.h>

namespace aether {

// shortcuts for engine implementation
#define lock_on() std::lock_guard<std::mutex> _lock_(mutex)

BinaryEngineImpl::BinaryEngineImpl(ArchType type, FileType os, EventConfig cfg,
                                   BinaryEngine *engine)
    : eventConf(cfg), arch(type) {
  using enum remill::ArchName;
  using enum remill::OSName;
  if (cfg.debug) {
    dbgContext.engine = engine;
    startDebugger(type);
  }
  // use windows coff as our in-memory object anyway
  remill::OSName os_name = kOSWindows;
  remill::ArchName arch_name;
  // lazily load handlers, only support AArch64 and X86_64
  if (arch == ARM64) {
    Handler::loadAArch64();
    arch_name = kArchAArch64LittleEndian;
  } else {
    Handler::loadX86();
    arch_name = kArchAMD64_AVX512;
  }
  remillArch = remill::Arch::Get(llvmContext, os_name, arch_name);
  remillSemantic = remill::LoadArchSemantics(
      remillArch.get(), {fs::path(self_path()).parent_path() / "bitcode"});
  CPU.runtime = this;
  // remove all the handlers' definition as we have built them into AetherVM
  // itself, and rename ISEL handler to the final one we need
  Lifter::resetSemantic(*remillSemantic);
}

BinaryEngineImpl::~BinaryEngineImpl() { CPU.freeContext(); }

static void *retaddr_aarch64() {
  auto lr = const_cast<RegisterValue *>(CPU.getRegisterAArch64(Register::LR));
  // the return address storage pointer
  return &lr->u8;
}

static void *retaddr_x86() {
  auto sp = const_cast<RegisterValue *>(CPU.getRegisterX86(Register::SP));
  sp->uptr--;
  // the return address storage pointer
  return &sp->uptr[0];
}

bool BinaryEngineImpl::startVM(addr_t entry) {
  if (!CPU.initContext(entry))
    return false;

  auto retaddr = arch == ARM64 ? retaddr_aarch64 : retaddr_x86;
  auto state = arch == ARM64 ? (void *)&CPU.aarch64 : (void *)&CPU.x86;
  auto insn = Orchestrator::inst()->find(entry);

#if AETHER_ARCH_ARM64
  aarch64::aether_vm_entry(state, entry, insn, &CPU.retaddr, retaddr);
#else
  x86::aether_vm_entry(state, entry, insn, &CPU.retaddr, retaddr);
#endif
  return true;
}

void thread_handler(uintptr_t *pcptr) {}
void insn_handler(void *state, uintptr_t pc, const void *insn) {}

void BinaryEngineImpl::startDebugger(ArchType type) {
  dbgContext.port = eventConf.dbgport;
  dbgContext.arm64 = type == ARM64;

  auto path = fs::path(self_path());
  path.replace_filename("libAetherDbg" + path.extension().string());
  auto handle = load_library(path.string());
  if (handle) {
    auto main = (aether_dbgmain_t)resolve_symbol(handle, "aether_dbgmain");
    // initialize debugger handler callbacks and start debugging daemon thread
    main(&dbgContext);
    return;
  }
  // default handlers
  dbgContext.thread_handler = thread_handler;
  dbgContext.insn_handler = insn_handler;
}

} // namespace aether

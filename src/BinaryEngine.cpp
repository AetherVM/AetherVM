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

BinaryEngineImpl::BinaryEngineImpl(ArchType type, FileType os) : arch(type) {
  using enum remill::ArchName;
  using enum remill::OSName;
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

static void init_context_aarch64(uintptr_t retaddr) {
  auto lr = CPU.getRegisterAArch64(Register::LR);
  // simulate a "bl blr" so that "ret" can work properly
  lr->u8p[0] = retaddr;
}

static void init_context_x86(uintptr_t retaddr) {
  auto sp = CPU.getRegisterX86(Register::SP);
  // simulate a "call/push-retaddr" so that "ret" can work properly
  sp->pptr[0]--;
  sp->pptr[0][0] = retaddr;
}

bool BinaryEngineImpl::startVM(addr_t entry) {
  if (!CPU.initContext(entry))
    return false;

  auto init_context = arch == ARM64 ? init_context_aarch64 : init_context_x86;
  auto insn = Orchestrator::inst()->find(entry);

#if AETHER_ARCH_ARM64
  aarch64::aether_vm_entry(&CPU, entry, insn, &CPU.retaddr, init_context);
#else
  x86::aether_vm_entry(&CPU, entry, insn, &CPU.retaddr, init_context);
#endif
  return true;
}

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <AetherBinary.h>
#include <AetherVM.h>
#include <Disassembler.h>
#include <Utils.h>

#include "BinaryEngine.h"
#include "Handler.h"
#include "Orchestrator.h"

#include <llvm/MC/MCInst.h>

namespace aether {

// shortcuts for engine implementation stub
#define engine (m_impl.get())
#define callbacks (engine->eventCallbacks)
#define memory (engine->guestMemory)

BinaryEngine::BinaryEngine(const Machine *mach, EventConfig eventcfg)
    : m_machine(mach) {
#if AETHER_OS_MACOS
  auto os = MachO;
#elif AETHER_OS_LINUX
  auto os = ELF;
#elif AETHER_OS_WINDOWS
  auto os = PE;
#else
#error AetherVM only supports macOS, Linux, and Windows
#endif
  m_impl =
      std::make_unique<BinaryEngineImpl>(mach->archType(), os, eventcfg, this);
}

BinaryEngine::BinaryEngine(const Binary *bin, EventConfig eventcfg)
    : m_binary(bin) {
  m_impl = std::make_unique<BinaryEngineImpl>(bin->archType(), bin->fileType(),
                                              eventcfg, this);
  // treat this bin as the main binary, so use the image base as guest base
  // memory address if it's not 0
  if (bin->imageBase())
    memory.baseGuest = bin->imageBase();
  else
    engine->vmBase = memory.guestAvailable();
  orchBinary(bin, engine->vmBase);
}

BinaryEngine::~BinaryEngine() {}

bool BinaryEngine::execute(std::span<const uint8_t> raw) {
  llvm::Module module("aethervm-object", engine->remillSemantic->getContext());
  engine->remillArch->PrepareModule(&module);
  // create a temporary memory object from raw
  auto memobj = Lifter::createObject(module, raw);
  auto bin = aether::New(std::move(memobj));
  if (!bin)
    return false;

  addr_t entry = 0;
  auto funcs = bin->functions();
  if (funcs.size()) {
    auto vmbase = memory.guestAvailable();
    // the real entry after map
    entry = vmbase + bin->functions().begin()->first - bin->imageBase();
    orchBinary(bin, vmbase);
  }
  aether::Delete(bin);

  return entry ? execute(entry) : false;
}

bool BinaryEngine::execute(addr_t target) {
  target += engine->vmBase;

  if (!memory.valid(target, 1))
    return false;

  return engine->startVM(target);
}

bool BinaryEngine::runMain() { return false; }

const void *BinaryEngine::makeExecutable(std::span<const uint8_t> raw) {
  return nullptr;
}

const RegisterValue *BinaryEngine::getRegister(Register reg) {
  return engine->arch == ARM64 ? CPU.getRegisterAArch64(reg)
                               : CPU.getRegisterX86(reg);
}

bool BinaryEngine::setRegister(Register reg, RegisterValue val) {
  // automatically set and managed by each thread cpu state
  if (reg == Register::SP)
    return false;
  return engine->arch == ARM64 ? CPU.setRegisterAArch64(reg, val)
                               : CPU.setRegisterX86(reg, val);
}

addr_t BinaryEngine::mapMemory(size_t size) {
  auto vmaddr = memory.guestAvailable();
  size = align_up(size, page_size());
  return memory.commit(vmaddr, size, true, true) ? vmaddr : 0;
}

std::vector<uint8_t> BinaryEngine::readMemory(addr_t addr, size_t size) {
  std::vector<uint8_t> buff;
  if (memory.valid(addr, size)) {
    buff.resize(size);
    std::memcpy(buff.data(), reinterpret_cast<void *>(memory.host(addr)), size);
  }
  return buff;
}

uint64_t BinaryEngine::readUInt64(addr_t addr) {
  uint64_t result = 0;
  if (memory.valid(addr, sizeof(result)))
    std::memcpy(&result, reinterpret_cast<void *>(memory.host(addr)),
                sizeof(result));
  return result;
}

bool BinaryEngine::writeMemory(addr_t addr, std::span<const uint8_t> buff) {
  if (!memory.valid(addr, buff.size()))
    return false;
  std::memcpy(reinterpret_cast<void *>(memory.host(addr)), buff.data(),
              buff.size());
  return true;
}

int BinaryEngine::registerCallback(EventCallback callback) {
  callbacks.push_back(callback);
  return (int)callbacks.size();
}

void BinaryEngine::liftOpcodes(const Binary *bin,
                               std::span<const uint8_t> opcodes) {
  llvm::MCInst inst;
  Lifter lifter{bin, const_cast<remill::Arch *>(engine->remillArch.get()),
                engine->remillSemantic.get()};
  if (bin->archType() == ARM64) {
    // arm64 has fixed 4 bytes instruction set
    constexpr size_t oplen = 4;
    Disassembler diser{Binary::arch(ARM64)};
    for (auto ptr = opcodes.data(), end = ptr + opcodes.size(); ptr < end;
         ptr += oplen) {
      if (diser.disassemble(ptr, 16, inst) == oplen)
        lifter.transform({ptr, oplen});
    }
  } else {
    Disassembler diser{Binary::arch(X86_64)};
    MachineX86 mx86;
    // iterate each x86 instruction
    for (auto ptr = opcodes.data(), end = ptr + opcodes.size(); ptr < end;) {
      size_t oplen = diser.disassemble(ptr, 16, inst);
      if (oplen)
        lifter.transform({ptr, oplen});
      else
        oplen = mx86.defaultSize();
      ptr += oplen;
    }
  }
}

void BinaryEngine::orchBinary(const Binary *bin, addr_t addend) {
  // calculate the total page size of all the sections
  size_t size = 0;
  for (auto &[addr, sect] : bin->sections())
    size += sect.size;
  size = align_up(size, page_size());

  // map all the sections
  memory.commit(bin->imageBase() + addend, size, true, true);
  for (auto &[addr, sect] : bin->sections()) {
    if (!sect.size)
      continue;
    auto sectbuff = reinterpret_cast<const uint8_t *>(bin->addrBuff(sect.addr));
    writeMemory(sect.addr + addend, {sectbuff, sect.size});
    if (sect.type == TEXT)
      liftOpcodes(bin, {sectbuff, sect.size});
  }

  Orchestrator::inst()->encode(bin, addend, engine->eventConf);
}

} // namespace aether

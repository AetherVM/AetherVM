// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "Handler.h"
#include <AetherBinary.h>

#include <llvm/IR/Module.h>
#include <llvm/MC/MCInst.h>
#include <llvm/Support/MemoryBuffer.h>

#include <remill/Arch/Arch.h>
#include <remill/Arch/Name.h>
#include <remill/BC/InstructionLifter.h>

#include <map>
#include <set>
#include <span>

namespace aether {

struct HandlerDynamic {
  // the raw opcode
  union {
    uint64_t opc8;
    uint32_t opc4;     // for arm64
    uint8_t opc16[16]; // for x86_64
  };
  // the dynamic handler entry of this raw opcode
  uintptr_t entry;

  HandlerDynamic() { std::memset(this, 0, sizeof(*this)); }

  bool operator<(const HandlerDynamic &rhs) const {
    return std::memcmp(&opc4, &rhs.opc4, std::size(opc16)) < 0;
  }
  bool operator==(const HandlerDynamic &rhs) const {
    return std::memcmp(&opc4, &rhs.opc4, std::size(opc16)) == 0;
  }
};

struct Lifter {
  // in-memory dynamic handler pages
  static std::map<uintptr_t, size_t> dynhandlers;

  static std::set<HandlerDynamic> aarch64;
  static std::set<HandlerDynamic> x86;

  const Binary *bin = nullptr;
  remill::Arch *arch = nullptr;
  const std::vector<Handler> *isel_handlers = nullptr;
  std::set<HandlerDynamic> *handlers = nullptr;
  std::map<std::string, HandlerDynamic *> name_handlers;
  llvm::Module *module = nullptr;

  Lifter(const Binary *bin, remill::Arch *ptr, llvm::Module *pre);
  ~Lifter();

  static void resetSemantic(llvm::Module &M);
  static std::unique_ptr<llvm::MemoryBuffer>
  createObject(llvm::Module &M, std::span<const uint8_t> text);
  void transform(const llvm::MCInst &Inst, std::span<const uint8_t> opcode);

private:
  void emitAArch64(llvm::Function &Func, const llvm::MCInst &Inst,
                   std::span<const uint8_t> opcode);
  void emitX64(llvm::Function &Func, const llvm::MCInst &Inst,
               std::span<const uint8_t> opcode);
  void apply(llvm::MemoryBuffer *mbuf);
  void clear();
};

} // namespace aether

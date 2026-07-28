// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "Handler.h"

#include <llvm/IR/Module.h>
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
    uint32_t opc4;     // for arm64
    uint8_t opc16[16]; // for x86_64
  };
  // the dynamic handler entry of this raw opcode
  uintptr_t entry;
  // the length of the opcode
  size_t oplen;

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

  remill::Arch *arch = nullptr;
  const std::vector<Handler> *isel_handlers = nullptr;
  std::set<HandlerDynamic> *handlers = nullptr;
  std::map<std::string, HandlerDynamic *> name_handlers;
  llvm::Module *module = nullptr;

  Lifter(remill::Arch *ptr, llvm::Module *pre);
  ~Lifter();

  static void resetSemantic(llvm::Module &M);
  void transform(std::span<const uint8_t> opcode);

private:
  void apply(llvm::MemoryBuffer *mbuf);
  void clear();
};

} // namespace aether

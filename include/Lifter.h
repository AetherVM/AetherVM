// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <llvm/IR/Module.h>
#include <map>
#include <remill/Arch/Arch.h>
#include <remill/Arch/Name.h>
#include <remill/BC/InstructionLifter.h>
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
  uint32_t oplen;
  // the size of the dynamic handler
  uint32_t entsize;

  HandlerDynamic() { std::memset(this, 0, sizeof(*this)); }

  bool operator<(const HandlerDynamic &rhs) const {
    return std::memcmp(&opc4, &rhs.opc4, std::size(opc16)) < 0;
  }
  bool operator==(const HandlerDynamic &rhs) const {
    return std::memcmp(&opc4, &rhs.opc4, std::size(opc16)) == 0;
  }
};

struct Lifter {
  // callee stubs of dynamic handlers for raw handlers
  static std::vector<uintptr_t> stubs;
  static size_t stub_pageoff;

  // in-memory object as dynamic handler container
  static std::map<uintptr_t, size_t> objects;

  static std::set<HandlerDynamic> arch64;
  static std::set<HandlerDynamic> x86;

  remill::Arch *arch = nullptr;
  std::set<HandlerDynamic> *handlers = nullptr;
  llvm::Module *module = nullptr;

  Lifter(remill::Arch *ptr, llvm::Module *pre);
  ~Lifter();

  void transform(std::span<const uint8_t> opcode);

private:
  void apply(uintptr_t pagestart, size_t pagesize);
  void clear();
};

} // namespace aether

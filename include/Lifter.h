// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <llvm/IR/Module.h>
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
  static std::set<HandlerDynamic> arch64;
  static std::set<HandlerDynamic> x86;

  remill::Arch *arch = nullptr;
  std::set<HandlerDynamic> *handlers = nullptr;
  const llvm::Module *prebuilt = nullptr;
  llvm::Module module;

  Lifter(remill::Arch *ptr, const llvm::Module *pre);
  ~Lifter();

  void transform(std::span<const uint8_t> opcode);
};

} // namespace aether

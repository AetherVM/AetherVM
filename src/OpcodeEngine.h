// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "BinaryEngine.h"

#include <Utils.h>

#include <set>
#include <span>

namespace aether {

template <typename T> struct OpcodeHandler {
  T opcode;

  auto operator<=>(const OpcodeHandler &right) const {
    return opcode <=> right.opcode;
  }
  bool operator==(const OpcodeHandler &right) const = default;

  void init();
  bool execute() const;
};

template <typename T> struct OpcodeHandlers {
  std::set<OpcodeHandler<T>> handlers;
  const OpcodeHandler<T> *caches[0xFF]{nullptr};

  bool emulate(T opcode) {
    auto id = fib_hash8(opcode);

    // lookup caches
    auto ptr = caches[id];
    if (ptr && ptr->opcode == opcode)
      return ptr->execute();

    // lookup existing handlers
    auto tmpopc = OpcodeHandler<T>{.opcode = opcode};
    auto found = handlers.find(tmpopc);
    if (found == handlers.end()) {
      // create a new handler
      found = handlers.insert(tmpopc).first;
      const_cast<OpcodeHandler<T> *>(&*found)->init();
    }

    // update caches
    caches[id] = &*found;
    return found->execute();
  }
};

struct OpcodeEngine {
  OpcodeHandlers<uint8_t> opc1;
  OpcodeHandlers<uint16_t> opc2;
  OpcodeHandlers<uint32_t> opc4;
  OpcodeHandlers<uint64_t> opc8;
  OpcodeHandlers<uint128_var_t> opc16;

  bool emulate(uint8_t opcode) { return opc1.emulate(opcode); }
  bool emulate(uint16_t opcode) { return opc2.emulate(opcode); }
  bool emulate(uint32_t opcode) { return opc4.emulate(opcode); }
  bool emulate(uint64_t opcode) { return opc8.emulate(opcode); }
  bool emulate(std::span<const uint8_t> opcode);
};

// emulation engine for each thread
extern thread_local OpcodeEngine EMU;

} // namespace aether

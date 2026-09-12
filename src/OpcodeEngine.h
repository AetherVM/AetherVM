// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <Utils.h>

#include <map>
#include <set>
#include <span>
#include <vector>

namespace remill {
class Instruction;
}

namespace aether {

template <typename T> struct OpcodeHandler {
  // opcode handler type
  enum Type {
    OHT_Remill,         // interpreted by remill
    OHT_Dynamic,        // dynamically generated on page-wx system
    OHT_Prebuit,        // prebuilt handlers on no-page-wx system like iOS
    OHT_PrebuiltMapped, // prebuilt handlers with register mapped
  };

  T opcode;
  Type type;
  std::vector<uint64_t> args;

#if AETHER_OS_DARWIN_IOS
  std::map<uint8_t, uint8_t> gpr, fpu;
#endif

  auto operator<=>(const OpcodeHandler &right) const {
    return opcode <=> right.opcode;
  }
  bool operator==(const OpcodeHandler &right) const = default;

  bool init();
  void init(remill::Instruction *inst);
  bool execute() const;

private:
  void initRemill(remill::Instruction *inst);
  void initDynamic();
  void initPrebuilt();
};

template <typename T> struct OpcodeHandlers {
  std::set<OpcodeHandler<T>> handlers;
  const OpcodeHandler<T> *caches[0xFF]{nullptr};

  void prefetch(remill::Instruction *inst, T opcode) {
    auto tmpopc = OpcodeHandler<T>{.opcode = opcode};
    auto found = handlers.find(tmpopc);
    if (found == handlers.end()) {
      // create a new handler
      found = handlers.insert(tmpopc).first;
      const_cast<OpcodeHandler<T> *>(&*found)->init(inst);
      // update caches
      caches[fib_hash8(opcode)] = &*found;
    }
  }

  bool emulate(T opcode, bool readonly) {
    auto id = fib_hash8(opcode);

    // lookup caches
    auto ptr = caches[id];
    if (ptr && ptr->opcode == opcode)
      return ptr->execute();

    // lookup existing handlers
    auto tmpopc = OpcodeHandler<T>{.opcode = opcode};
    auto found = handlers.find(tmpopc);
    if (found == handlers.end()) {
      if (readonly)
        return false;

      // create a new handler
      found = handlers.insert(tmpopc).first;
      if (!const_cast<OpcodeHandler<T> *>(&*found)->init())
        return false; // invalid instruction
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
  bool readonly = false; // don't create new handler during emulation

  size_t prefetch(std::span<const uint8_t> opcodes);

  bool emulate(uint8_t opcode) { return opc1.emulate(opcode, readonly); }
  bool emulate(uint16_t opcode) { return opc2.emulate(opcode, readonly); }
  bool emulate(uint32_t opcode) { return opc4.emulate(opcode, readonly); }
  bool emulate(uint64_t opcode) { return opc8.emulate(opcode, readonly); }
  bool emulate(std::span<const uint8_t> opcode);
};

// emulation engine for each thread
extern thread_local OpcodeEngine EMU;

} // namespace aether

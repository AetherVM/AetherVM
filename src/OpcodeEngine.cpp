// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "OpcodeEngine.h"

namespace aether {

thread_local OpcodeEngine EMU;

template <typename T> void OpcodeHandler<T>::init() {}

template <typename T> bool OpcodeHandler<T>::execute() const { return false; }

template struct OpcodeHandler<uint8_t>;
template struct OpcodeHandler<uint16_t>;
template struct OpcodeHandler<uint32_t>;
template struct OpcodeHandler<uint64_t>;
template struct OpcodeHandler<uint128_var_t>;

bool OpcodeEngine::emulate(std::span<const uint8_t> opcode) {
  uint32_t tmp4{0};
  uint64_t tmp8{0};
  switch (opcode.size()) {
  case 1:
    return emulate(*(uint8_t *)opcode.data());
  case 2:
    return emulate(*(uint16_t *)opcode.data());
  case 3:
    std::memcpy(&tmp4, opcode.data(), 3);
    return emulate(tmp4);
  case 4:
    return emulate(*(uint32_t *)opcode.data());
  case 5:
    std::memcpy(&tmp8, opcode.data(), 5);
    return emulate(tmp8);
  case 6:
    std::memcpy(&tmp8, opcode.data(), 6);
    return emulate(tmp8);
  case 7:
    std::memcpy(&tmp8, opcode.data(), 7);
    return emulate(tmp8);
  case 8:
    return emulate(*(uint64_t *)opcode.data());
  default:
    break;
  }
  uint128_var_t tmp16;
  std::memcpy(&tmp16, opcode.data(), std::min((size_t)16, opcode.size()));
  return opc16.emulate(tmp16);
}

} // namespace aether

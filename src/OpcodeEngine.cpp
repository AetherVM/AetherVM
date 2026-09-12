// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "OpcodeEngine.h"
#include "BinaryEngine.h"

// shortcuts for engine implementation stub
#define engine (CPU.runtime)

namespace aether {

thread_local OpcodeEngine EMU;

template <typename T> bool OpcodeHandler<T>::init() {
  std::lock_guard<std::mutex> lock(engine->mutex);

  remill::Instruction inst;
  auto arch = engine->remillArch.get();
  std::ignore =
      arch->DecodeInstruction(0, {(char *)&opcode, (char *)&opcode + sizeof(T)},
                              inst, arch->CreateInitialContext());
  if (inst.bytes.size() == 0)
    return false;

  init(inst.IsValid() ? &inst : nullptr);
  return true;
}

template <typename T> void OpcodeHandler<T>::init(remill::Instruction *inst) {
  if (inst) {
    initRemill(inst);
  } else {
#if AETHER_OS_DARWIN_IOS
    initPrebuilt();
#else
    initDynamic();
#endif
  }
}

template <typename T>
void OpcodeHandler<T>::initRemill(remill::Instruction *inst) {}

template <typename T> void OpcodeHandler<T>::initDynamic() {}

template <typename T> void OpcodeHandler<T>::initPrebuilt() {}

template <typename T> bool OpcodeHandler<T>::execute() const { return false; }

template struct OpcodeHandler<uint8_t>;
template struct OpcodeHandler<uint16_t>;
template struct OpcodeHandler<uint32_t>;
template struct OpcodeHandler<uint64_t>;
template struct OpcodeHandler<uint128_var_t>;

size_t OpcodeEngine::prefetch(std::span<const uint8_t> opcodes) {
  size_t count = 0;
  auto ptr = (const char *)opcodes.data();
  auto endptr = ptr + opcodes.size();
  auto arch = engine->remillArch.get();
  remill::Instruction inst;
  while (ptr < endptr) {
    std::ignore = arch->DecodeInstruction(0, {ptr, endptr}, inst,
                                          arch->CreateInitialContext());
    if (inst.bytes.size() == 0) {
      ptr += arch->arch_name == remill::kArchAArch64LittleEndian ? 4 : 1;
      continue;
    }

    auto instptr = inst.IsValid() ? &inst : nullptr;
    uint32_t tmp4{0};
    uint64_t tmp8{0};
    uint128_var_t tmp16{0, 0};
    switch (inst.bytes.size()) {
    case 1:
      opc1.prefetch(instptr, *(uint8_t *)ptr);
      break;
    case 2:
      opc2.prefetch(instptr, *(uint16_t *)ptr);
      break;
    case 3:
      std::memcpy(&tmp4, ptr, 3);
      opc4.prefetch(instptr, tmp4);
      break;
    case 4:
      opc4.prefetch(instptr, *(uint32_t *)ptr);
      break;
    case 5:
      std::memcpy(&tmp8, ptr, 5);
      opc8.prefetch(instptr, tmp8);
      break;
    case 6:
      std::memcpy(&tmp8, ptr, 6);
      opc8.prefetch(instptr, tmp8);
      break;
    case 7:
      std::memcpy(&tmp8, ptr, 7);
      opc8.prefetch(instptr, tmp8);
      break;
    case 8:
      opc8.prefetch(instptr, *(uint64_t *)ptr);
      break;
    default:
      std::memcpy(&tmp16, ptr, std::min((size_t)16, inst.bytes.size()));
      opc16.prefetch(instptr, tmp16);
      break;
    }
    count++;
    ptr += inst.bytes.size();
  }
  return count;
}

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
  uint128_var_t tmp16{0, 0};
  std::memcpy(&tmp16, opcode.data(), std::min((size_t)16, opcode.size()));
  return opc16.emulate(tmp16, readonly);
}

} // namespace aether

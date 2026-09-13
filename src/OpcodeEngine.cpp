// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "OpcodeEngine.h"
#include "BinaryEngine.h"
#include "Handler.h"
#include "Lifter.h"
#include "Orchestrator.h"

#include <Platform.h>

// shortcuts for engine implementation stub
#define engine (CPU.runtime)

namespace aether {

thread_local OpcodeEngine EMU;

#if !AETHER_OS_DARWIN_IOS
template <typename T> std::vector<uint64_t> OpcodeHandler<T>::dynhandlers;
template <typename T> uint8_t *OpcodeHandler<T>::pagestart = nullptr;
template <typename T> uint8_t *OpcodeHandler<T>::pagecur = nullptr;
#endif

template <typename T> std::vector<remill::Operand> OpcodeHandler<T>::operands;
template <typename T>
std::map<uint64_t, const remill::Operand *> OpcodeHandler<T>::operandmap;

class Operand : public remill::Operand {
public:
  uint64_t ID() const {
    using remill::Operand;
    uint64_t id = 0;
    switch (type) {
    case kTypeInvalid:
      break;
    case kTypeRegister:
      std::memcpy(&id, reg.name.data(), std::min((size_t)8, reg.name.size()));
      break;
    case kTypeShiftRegister:
      std::memcpy(&id, shift_reg.reg.name.data(),
                  std::min((size_t)8, shift_reg.reg.name.size()));
      id |= ((uint64_t)hash_value(
                 {(char *)&shift_reg.shift_size,
                  (char *)shift_reg.extend_op + sizeof(shift_reg.extend_op)})
             << 24);
      break;
    case kTypeImmediate:
      id |= ((imm.val << 5) >> 5);
      id |= ((uint64_t)imm.is_signed << 59);
      break;
    case kTypeAddress:
      id |= hash_value(addr.segment_base_reg.name + addr.base_reg.name +
                       addr.index_reg.name);
      id ^= ((uint64_t)(uint32_t)hash_value(
                 {(char *)&addr.scale, (char *)&addr.kind + sizeof(addr.kind)})
             << 28);
      break;
    case kTypeExpression:
    case kTypeRegisterExpression:
    case kTypeImmediateExpression:
    case kTypeAddressExpression:
      id |= (uint64_t)expr;
      break;
    default:
      abort();
    }
    return id | (((uint64_t)type) << 60);
  }
};

template <typename T> bool OpcodeHandler<T>::init() {
  std::lock_guard<std::mutex> lock(engine->mutex);

  remill::Instruction inst;
  auto arch = engine->remillArch.get();
  std::ignore =
      arch->DecodeInstruction(0, {(char *)&opcode, (char *)&opcode + sizeof(T)},
                              inst, arch->CreateInitialContext());
  if (inst.bytes.size() == 0)
    return false;

  init(inst);
  return true;
}

template <typename T> void OpcodeHandler<T>::init(remill::Instruction &inst) {
  if (inst.IsValid()) {
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
void OpcodeHandler<T>::initRemill(remill::Instruction &inst) {
  auto arch = engine->remillArch.get();
  auto handlers = arch->arch_name == remill::kArchAArch64LittleEndian
                      ? &Handler::aarch64
                      : &Handler::x86;
  Handler key{hash_value(inst.function), nullptr};
  auto base = handlers->data();
  auto found = binary_search(base, handlers->size(), key);
  if (is_exact(found, base, handlers->size(), key)) {
    // set implementation
    impl = found->impl;
    type = OHT_Remill;
  } else {
    inst.category = remill::Instruction::kCategoryInvalid;
    init(inst);
    return;
  }
  for (auto &o : inst.operands) {
    auto optr = (Operand *)&o;
    auto id = optr->ID();
    auto found = operandmap.find(id);
    if (found == operandmap.end()) {
      operands.push_back(o);
      found = operandmap.insert(std::make_pair(id, &*operands.rbegin())).first;
    }
    // set operands
    args.push_back(found->second);
  }
}

template <typename T> void OpcodeHandler<T>::initDynamic() {
#if AETHER_OS_DARWIN_IOS
  abort();
#else
  llvm::MCInst inst;
  auto oplen =
      engine->diser.disassemble((uint8_t *)&opcode, sizeof(opcode), inst);
  type = OHT_Dynamic;
  if (!oplen) {
    impl = (void *)&abort;
    return;
  }
  auto arch = engine->remillArch.get();
  auto nativeHandler = arch->arch_name == remill::kArchAArch64LittleEndian
                           ? Lifter::nativeHandlerAArch64
                           : Lifter::nativeHandlerX64;
  auto asmbody =
      nativeHandler(inst, {(uint8_t *)&opcode, (uint8_t *)&opcode + oplen});
  uint8_t newopc[20], asmbin[256];
  intptr_t binsz = 0, pagesz = page_size();
  for (auto &insn : string_view_split(asmbody, '\n')) {
    if (insn.size() == 0)
      continue;
    // force to reset '\n' to '\0', we're reusing asmbody's buffer
    const_cast<char *>(insn.data() + insn.size())[0] = 0;

    newopc[0] = 0;
    engine->diser.assemble(insn.data(), newopc);
    if (!newopc[0]) {
      // should never happend
      abort();
    }
    std::memcpy(&asmbin[binsz], &newopc[1], newopc[0]);
    binsz += newopc[0];
    assert(binsz < (intptr_t)sizeof(asmbin));
  }

  if (!pagestart || pagestart + pagesz - pagecur < binsz) {
    dynhandlers.push_back(page_alloc(pagesz));
    pagestart = (uint8_t *)*dynhandlers.rbegin();
    pagecur = pagestart;
  }
  // rw-
  page_commit(pagestart, pagesz, true, true, false);
  impl = pagecur;
  std::memcpy(pagecur, &asmbin[0], binsz);
  pagecur += binsz;
  // r-x
  page_commit(pagestart, pagesz, true, false, true);
#endif
}

template <typename T> void OpcodeHandler<T>::initPrebuilt() {
#if AETHER_OS_DARWIN_IOS
  type = OHT_Prebuit;
#else
  abort();
#endif
}

template <typename T> bool OpcodeHandler<T>::interpRemill() const {
  return true;
}

static void *vm_retaddr() {
  // reused as a temporary return address
  return &CPU.rvalue;
}

template <typename T> void OpcodeHandler<T>::execDynamic() const {
  Instruction insns[2]{{(event_func_t)impl}, {finish_emulation}};
  auto arch = engine->remillArch.get();
  auto state = arch->arch_name == remill::kArchAArch64LittleEndian
                   ? (void *)&CPU.aarch64
                   : (void *)&CPU.x86;
  auto entry = *CPU.pcptr;
#if AETHER_ARCH_ARM64
  aarch64::aether_vm_entry(state, entry, insns, &CPU.retaddr, vm_retaddr);
#else
  x86::aether_vm_entry(state, entry, insns, &CPU.retaddr, vm_retaddr);
#endif
}

template <typename T> void OpcodeHandler<T>::execPrebuilt() const {}

template <typename T> void OpcodeHandler<T>::execPrebuiltMapped() const {}

template <typename T> bool OpcodeHandler<T>::interpret() const {
  switch (type) {
  case OHT_Remill:
    return interpRemill();
  case OHT_Dynamic:
    execDynamic();
    break;
  case OHT_Prebuit:
    execPrebuilt();
    break;
  case OHT_PrebuiltMapped:
    execPrebuiltMapped();
    break;
  default:
    return false;
  }
  return true;
}

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

    uint32_t tmp4{0};
    uint64_t tmp8{0};
    uint128_var_t tmp16{0, 0};
    switch (inst.bytes.size()) {
    case 1:
      opc1.prefetch(inst, *(uint8_t *)ptr);
      break;
    case 2:
      opc2.prefetch(inst, *(uint16_t *)ptr);
      break;
    case 3:
      std::memcpy(&tmp4, ptr, 3);
      opc4.prefetch(inst, tmp4);
      break;
    case 4:
      opc4.prefetch(inst, *(uint32_t *)ptr);
      break;
    case 5:
      std::memcpy(&tmp8, ptr, 5);
      opc8.prefetch(inst, tmp8);
      break;
    case 6:
      std::memcpy(&tmp8, ptr, 6);
      opc8.prefetch(inst, tmp8);
      break;
    case 7:
      std::memcpy(&tmp8, ptr, 7);
      opc8.prefetch(inst, tmp8);
      break;
    case 8:
      opc8.prefetch(inst, *(uint64_t *)ptr);
      break;
    default:
      std::memcpy(&tmp16, ptr, std::min((size_t)16, inst.bytes.size()));
      opc16.prefetch(inst, tmp16);
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

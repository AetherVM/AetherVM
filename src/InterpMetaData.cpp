// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "InterpMetaData.h"
#include "BinaryEngine.h"
#include "CPUState.h"
#include "Orchestrator.h"

#include <Platform.h>
#include <Utils.h>

#include <algorithm>
#include <cstring>

#include <llvm/IR/Instruction.h>

#if AETHER_OS_DARWIN_IOS
#define IMETADATA_EXEC false
#else
#define IMETADATA_EXEC true
#endif

namespace aether {

InterpMetaData mdInterp;

namespace {

inline uint64_t MaskToBits(uint64_t value, unsigned bits) {
  if (bits == 0)
    return 0;
  if (bits >= 64)
    return value;
  return value & ((uint64_t{1} << bits) - 1);
}

inline uint64_t SignExtend(uint64_t value, unsigned bits) {
  if (bits == 0 || bits >= 64)
    return value;
  value = MaskToBits(value, bits);
  const uint64_t sign_bit = uint64_t{1} << (bits - 1);
  return (value ^ sign_bit) - sign_bit;
}

inline void *GetState() {
#if AETHER_ARCH_ARM64
  return &CPU.aarch64;
#else
  return &CPU.x86;
#endif
}

template <typename Register>
inline uint64_t LoadRegValueRaw(void *state, const Register &reg) {
  const auto *address = reinterpret_cast<const uint8_t *>(state) + reg.offset;
  const size_t num_bytes = (static_cast<size_t>(reg.size) + 7u) / 8u;
  if (num_bytes > sizeof(uint64_t))
    return reinterpret_cast<uint64_t>(address);

  uint64_t value = 0;
  std::memcpy(&value, address, num_bytes);
  return value;
}

template <typename Register>
inline uint64_t LoadWordRegValOrZeroRaw(void *state, const Register &reg,
                                        unsigned word_size_bits) {
  if (!reg.size)
    return 0;
  return MaskToBits(LoadRegValueRaw(state, reg), word_size_bits);
}

uint64_t LiftShiftRegisterOperandRaw(void *state,
                                     const IMetaData::Operand &operand) {
  const auto &shift = operand.value.shift_register;
  auto reg = LoadRegValueRaw(state, shift.reg);
  auto curr_size = static_cast<unsigned>(shift.reg.size);
  const auto word_size = sizeof(uintptr_t) * 8;

  const uint64_t zero = 0;
  const uint64_t one = 1;
  const uint64_t shift_size = shift.shift_size;

  using enum remill::Operand::ShiftRegister::Shift;
  using enum remill::Operand::ShiftRegister::Extend;
  if (kExtendInvalid != shift.extend_op) {
    const auto extract_size = static_cast<unsigned>(shift.extract_size);
    if (shift.reg.size > extract_size) {
      curr_size = extract_size;
      reg = MaskToBits(reg, extract_size);
    }

    if (operand.size > shift.extract_size) {
      switch (shift.extend_op) {
      case kExtendSigned:
        reg = SignExtend(reg, extract_size);
        curr_size = operand.size;
        break;
      case kExtendUnsigned:
        curr_size = operand.size;
        break;
      default:
        abort();
      }
    }
  }

  if (curr_size < operand.size) {
    reg = MaskToBits(reg, curr_size);
    curr_size = operand.size;
  }

  if (kShiftInvalid != shift.shift_op) {
    const auto op_bits = static_cast<unsigned>(operand.size);
    switch (shift.shift_op) {
    case kShiftLeftWithZeroes:
      reg = MaskToBits(reg << shift_size, op_bits);
      break;
    case kShiftLeftWithOnes: {
      const uint64_t mask_val = ~((~zero) << shift_size);
      reg = MaskToBits((reg << shift_size) | mask_val, op_bits);
      break;
    }
    case kShiftUnsignedRight:
      reg = MaskToBits(reg >> shift_size, op_bits);
      break;
    case kShiftSignedRight: {
      const auto signed_reg = static_cast<int64_t>(SignExtend(reg, op_bits));
      reg =
          MaskToBits(static_cast<uint64_t>(signed_reg >> shift_size), op_bits);
      break;
    }
    case kShiftLeftAround: {
      const uint64_t shr_amount = (~shift_size + one) & (operand.size - one);
      const auto val1 = reg >> shr_amount;
      const auto val2 = reg << shift_size;
      reg = MaskToBits(val1 | val2, op_bits);
      break;
    }
    case kShiftRightAround: {
      const uint64_t shl_amount = (~shift_size + one) & (operand.size - one);
      const auto val1 = reg >> shift_size;
      const auto val2 = reg << shl_amount;
      reg = MaskToBits(val1 | val2, op_bits);
      break;
    }
    case kShiftInvalid:
      break;
    }
  }

  if (word_size > operand.size)
    reg = MaskToBits(reg, static_cast<unsigned>(operand.size));
  return reg;
}

uint64_t LiftAddressOperandRaw(void *state, const IMetaData::Operand &operand) {
  const auto &address = operand.value.address;
  const auto word_size = sizeof(uintptr_t) * 8;
  auto segment = LoadWordRegValOrZeroRaw(state, address.regs[0], word_size);
  auto value = LoadWordRegValOrZeroRaw(state, address.regs[1], word_size);
  auto index = LoadWordRegValOrZeroRaw(state, address.regs[2], word_size);
  const auto scale = static_cast<uint64_t>(address.scale);

  if (index)
    value = MaskToBits(value + index * scale, word_size);

  if (address.displacement) {
    if (0 < address.displacement) {
      value = MaskToBits(value + static_cast<uint64_t>(address.displacement),
                         word_size);
    } else {
      value = MaskToBits(value - static_cast<uint64_t>(-address.displacement),
                         word_size);
    }
  }

  if (segment)
    value = MaskToBits(value + segment, word_size);

  if (address.address_size < word_size)
    value = MaskToBits(value, static_cast<unsigned>(address.address_size));
  return value;
}

uint64_t LiftExpressionOperandRecRaw(void *state,
                                     const IMetaData::Expression &expression,
                                     const IMetaData::Expression *expressions) {
  using MetadataExpression = IMetaData::Expression;
  switch (static_cast<MetadataExpression::Kind>(expression.kind)) {
  case MetadataExpression::kOperation: {
    const auto &operation = expression.payload.operation;
    const auto lhs = LiftExpressionOperandRecRaw(
        state, expressions[operation.lhs], expressions);
    uint64_t rhs = 0;
    if (operation.rhs != MetadataExpression::no_child) {
      rhs = LiftExpressionOperandRecRaw(state, expressions[operation.rhs],
                                        expressions);
    }

    switch (expression.opcode) {
    case llvm::Instruction::Add:
      return lhs + rhs;
    case llvm::Instruction::Sub:
      return lhs - rhs;
    case llvm::Instruction::Mul:
      return lhs * rhs;
    case llvm::Instruction::Shl:
      return lhs << rhs;
    case llvm::Instruction::LShr:
      return lhs >> rhs;
    case llvm::Instruction::AShr:
      return static_cast<uint64_t>(static_cast<int64_t>(lhs) >>
                                   static_cast<int64_t>(rhs));
    case llvm::Instruction::ZExt:
    case llvm::Instruction::Trunc:
      return MaskToBits(lhs, expression.width);
    case llvm::Instruction::SExt:
      return SignExtend(lhs, expression.width);
    case llvm::Instruction::And:
      return lhs & rhs;
    case llvm::Instruction::Or:
      return lhs | rhs;
    case llvm::Instruction::URem:
      return rhs ? lhs % rhs : 0;
    case llvm::Instruction::Xor:
      return lhs ^ rhs;
    default:
      abort();
    }
  }
  case MetadataExpression::kRegister:
    return LoadRegValueRaw(state, expression.payload.reg);
  case MetadataExpression::kConstant:
    return (static_cast<uint64_t>(expression.payload.constant.high) << 32) |
           expression.payload.constant.low;
  default:
    abort();
  }
}

uint64_t LiftOperandRaw(void *state, const IMetaData::Operand &operand,
                        const IMetaData::Expression *expressions) {
  using remill::Operand;
  switch (static_cast<Operand::Type>(operand.type)) {
  case Operand::kTypeRegister:
    return operand.action == Operand::kActionWrite
               ? reinterpret_cast<uint64_t>(reinterpret_cast<uint8_t *>(state) +
                                            operand.value.reg.offset)
               : LoadRegValueRaw(state, operand.value.reg);
  case Operand::kTypeShiftRegister:
    return LiftShiftRegisterOperandRaw(state, operand);
  case Operand::kTypeImmediate: {
    const auto value = operand.value.immediate.value;
    const auto bits = static_cast<unsigned>(operand.size);
    return operand.value.immediate.is_signed ? SignExtend(value, bits)
                                             : MaskToBits(value, bits);
  }
  case Operand::kTypeAddress:
    return LiftAddressOperandRaw(state, operand);
  case Operand::kTypeExpression:
  case Operand::kTypeRegisterExpression:
  case Operand::kTypeImmediateExpression:
  case Operand::kTypeAddressExpression:
    return LiftExpressionOperandRecRaw(state, operand.value.expression,
                                       expressions);
  default:
    abort();
  }
}

inline void SetNextPC(uint8_t oplen) {
#if AETHER_ARCH_ARM64
  CPU.aarch64.gpr._1 = CPU.pcptr[0] + oplen;
#else
  CPU.x86.gpr._1 = CPU.pcptr[0] + oplen;
#endif
}

inline void UpdatePC() {
#if AETHER_ARCH_ARM64
  CPU.pcptr[0] = CPU.aarch64.gpr._1;
#else
  CPU.pcptr[0] = CPU.x86.gpr._1;
#endif
}

inline uint64_t CallHost(uint64_t addr) {
#if AETHER_ARCH_ARM64
  aarch64::call_host(&CPU.aarch64.gpr.x0, &CPU.aarch64.simd.v[0], addr);
  return CPU.aarch64.gpr.x30.qword;
#else
  abort();
#endif
}

} // namespace

RuntimeMetaData::RuntimeMetaData(std::string_view path,
                                 std::string_view aevmpath, uint64_t rtbase,
                                 uint64_t rtend)
    : path(path), rtbase(rtbase), rtend(rtend) {
  metadata = reinterpret_cast<const IMetaData *>(
      map_file_readonly(aevmpath, &mdsize, IMETADATA_EXEC));
  insns = metadata->insns();

  if (metadata->flags & IMetaData::kFlag_ARM64)
    remillhandlers = &Handler::aarch64[0];
  else
    remillhandlers = &Handler::x86[0];

  nexticache.resize(metadata->num_instruction);
  for (size_t i = 0; i < nexticache.size(); ++i) {
    // Initialize the next instruction cache with sequential indices
    nexticache[i] = i + 1;

    auto handler = metadata->handler(&insns[i]);
    switch (handler->type) {
    case OpcodeHandler<uint32_t>::OHT_MCInst: {
      // make sure the implementation index matches the current size of mcinsts
      assert(handler->impl == mcinsts.size());

      llvm::MCInst mcinst;
      CPU.runtime->diser.disassemble(&handler->opcode[0], handler->oplen,
                                     mcinst);
      mcinsts.push_back(mcinst);
      break;
    }
    case OpcodeHandler<uint32_t>::OHT_Prebuilt:
    case OpcodeHandler<uint32_t>::OHT_PrebuiltMapped: {
#if AETHER_OS_DARWIN_IOS
      // make sure the implementation index matches the current size of
      // prebuiltinsts
      assert(handler->impl == prebuiltinsts.size());

      OpcodeHandler<uint32_t> opchandler;
      opchandler.opcode = *(uint32_t *)&handler->opcode[0];
      opchandler.oplen = 4;
      opchandler.preInitPrebuilt();
      prebuiltinsts.push_back(opchandler);
#else
      abort();
#endif
      break;
    }
    default:
      break;
    }
  }
  if (CPU.runtime->eventConf.debug)
    log_print(Runtime, "Loaded {}: 0x{:x} - 0x{:x}", path, rtbase, rtend);
}

RuntimeMetaData::~RuntimeMetaData() {
  if (path.empty())
    return;

  // Unmap the metadata file
  unmap_file(reinterpret_cast<uintptr_t>(metadata), mdsize);
}

const IMetaData::InsnMeta *RuntimeMetaData::find(uint64_t addr) const {
  IMetaData::InsnMeta key;
  key.rva = (uint32_t)(addr - rtbase);
  auto found = binary_search(insns, metadata->num_instruction, key);
  return found->rva == key.rva ? found : nullptr;
}

std::string InterpMetaData::add(std::string_view path, uint64_t rtbase,
                                uint64_t rtend) {
  auto aevm =
      (fs::path(cacheRoot) / fs::path(path).filename()).string() + ".aevm";
  if (!fs::exists(aevm))
    return std::format("{} does not exist", aevm);

  runtimes.emplace_back(path, aevm, rtbase, rtend);

  // Sort the runtimes vector in place by their base address (rtbase)
  std::sort(runtimes.begin(), runtimes.end());
  return "";
}

void InterpMetaData::reset() { runtimes.clear(); }

const RuntimeMetaData *
InterpMetaData::find(uint64_t addr, const IMetaData::InsnMeta **insn) const {
  RuntimeMetaData key;
  key.rtbase = addr;
  auto rtfound = binary_search(runtimes.data(), runtimes.size(), key);
  if (rtfound == runtimes.data() + runtimes.size())
    rtfound = &runtimes.back();
  if (addr > rtfound->rtend)
    return nullptr;
  if (insn)
    *insn = rtfound->find(addr);
  return rtfound;
}

int InterpMetaData::main(uint64_t entry, int argc, const char *argv[]) {
  const IMetaData::InsnMeta *insn = nullptr;
  auto runtime = find(entry, &insn);
  if (!runtime)
    return -1; // Entry not found in any runtime
  if (!insn)
    return -1; // Instruction not found in the runtime

  // Set up CPU registers for program arguments based on architecture and OS for
  // guest main function
#if AETHER_ARCH_ARM64
  auto result = []() { return CPU.getRegisterAArch64(Register::X0)->u4; };
  auto returnPtr = []() -> uint64_t * {
    auto lr = const_cast<RegisterValue *>(CPU.getRegisterAArch64(Register::LR));
    // the return address storage pointer
    return &lr->u8;
  };
  CPU.setRegisterAArch64(Register::X0, {.u8 = (uint64_t)argc});
  CPU.setRegisterAArch64(Register::X1, {.ptr = argv});
#else
  auto result = []() { return CPU.getRegisterX86(Register::RAX)->u4; };
  auto returnPtr = []() -> uint64_t * {
    auto sp = const_cast<RegisterValue *>(CPU.getRegisterX86(Register::SP));
#if AETHER_OS_WINDOWS
    sp->uptr -= (1 + 4); // 1 for return address and 4 shadow space
#else
    sp->uptr -= (1 + 0);
#endif
    // the return address storage pointer
    return &sp->uptr[0];
  };
#if AETHER_OS_WINDOWS
  CPU.setRegisterX86(Register::RCX, {.u8 = (uint64_t)argc});
  CPU.setRegisterX86(Register::RDX, {.ptr = argv});
#else
  CPU.setRegisterX86(Register::RDI, {.u8 = (uint64_t)argc});
  CPU.setRegisterX86(Register::RSI, {.ptr = argv});
#endif
#endif
  CPU.pcptr[0] = entry;
  // flag the top return address as this pointer
  *returnPtr() = (uint64_t)this;

  vmEntry(runtime, insn);
  return result();
}

void InterpMetaData::interpRemill(const IMetaData::Handler *handler,
                                  const IMetaData::Operand *operands,
                                  const IMetaData::Expression *expressions) {
#if AETHER_ARCH_ARM64
  auto handlerImpl = Handler::aarch64[handler->impl].impl;
#else
  auto handlerImpl = Handler::x86[handler->impl].impl;
#endif
  SetNextPC(handler->oplen);

  uint64_t params[16];
  auto state = GetState();
  params[0] = 0;                                 // memory
  params[1] = reinterpret_cast<uint64_t>(state); // cpu state
  auto i = 2;
  const auto *operand_indices = handler->operand();
  for (uint8_t arg = 0; arg < handler->oprnum; ++arg) {
    params[i++] =
        LiftOperandRaw(state, operands[operand_indices[arg]], expressions);
  }

#define CASE_N(N, ...)                                                         \
  case N:                                                                      \
    ((void (*)(__VA_ARGS__))handlerImpl)(PARAMS_##N);                          \
    break;

#define PARAMS_2 params[0], params[1]
#define PARAMS_3 PARAMS_2, params[2]
#define PARAMS_4 PARAMS_3, params[3]
#define PARAMS_5 PARAMS_4, params[4]
#define PARAMS_6 PARAMS_5, params[5]
#define PARAMS_7 PARAMS_6, params[6]
#define PARAMS_8 PARAMS_7, params[7]
#define PARAMS_9 PARAMS_8, params[8]
#define PARAMS_10 PARAMS_9, params[9]
#define PARAMS_11 PARAMS_10, params[10]
#define PARAMS_12 PARAMS_11, params[11]
#define PARAMS_13 PARAMS_12, params[12]
#define PARAMS_14 PARAMS_13, params[13]
#define PARAMS_15 PARAMS_14, params[14]

#define REPEAT_6(T) T, T, T, T, T, T
#define REPEAT_8(T) REPEAT_6(T), T, T
#define REPEAT_14(T) REPEAT_8(T), REPEAT_6(T)

  switch (i) {
    CASE_N(2, uint64_t, uint64_t)
    CASE_N(3, uint64_t, uint64_t, uint64_t)
    CASE_N(4, uint64_t, uint64_t, uint64_t, uint64_t)
    CASE_N(5, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
    CASE_N(6, REPEAT_6(uint64_t))
    CASE_N(7, REPEAT_6(uint64_t), uint64_t)
    CASE_N(8, REPEAT_8(uint64_t))
    CASE_N(9, REPEAT_8(uint64_t), uint64_t)
    CASE_N(10, REPEAT_8(uint64_t), uint64_t, uint64_t)
    CASE_N(11, REPEAT_8(uint64_t), uint64_t, uint64_t, uint64_t)
    CASE_N(12, REPEAT_6(uint64_t), REPEAT_6(uint64_t))
    CASE_N(13, REPEAT_6(uint64_t), REPEAT_6(uint64_t), uint64_t)
    CASE_N(14, REPEAT_14(uint64_t))
    CASE_N(15, REPEAT_14(uint64_t), uint64_t)
  default:
    abort();
  }
  UpdatePC();
}

#if AETHER_ARCH_ARM64
void interpMCInstARM64(const llvm::MCInst &inst);
#define interpMCInst(inst) interpMCInstARM64(inst)
#else
#define interpMCInst(inst) abort()
#endif

void InterpMetaData::vmEntry(const RuntimeMetaData *rt,
                             const IMetaData::InsnMeta *insn) {
  auto engine = CPU.runtime;
  auto dbgctx = (engine->eventConf.debug && engine->eventConf.watchdog)
                    ? &engine->dbgContext
                    : nullptr;
  auto pcptr = CPU.pcptr;
  auto metadata = rt->metadata;
  auto mdinsns = metadata->insns();
  auto operands = metadata->operands();
  auto expressions = metadata->expressions();
  auto rtstart = runtimes.begin()->rtbase;
  auto rtend = runtimes.rbegin()->rtend;
  while (pcptr[0] != (uint64_t)this) {
    // debug helper
    if (dbgctx)
      dbgctx->insn_handler(&CPU.aarch64, CPU.pcptr[0], nullptr);

    auto handler = metadata->handler(insn);
    switch (handler->type) {
    case OpcodeHandler<uint32_t>::OHT_Remill: {
      interpRemill(handler, operands, expressions);
      break;
    }
    case OpcodeHandler<uint32_t>::OHT_MCInst: {
      interpMCInst(rt->mcinsts[handler->impl]);
      break;
    }
#if AETHER_OS_DARWIN_IOS
    case OpcodeHandler<uint32_t>::OHT_Prebuilt: {
      auto &oneshot = rt->prebuiltinsts[handler->impl];
      oneshot.preExecPrebuilt();
      break;
    }
    case OpcodeHandler<uint32_t>::OHT_PrebuiltMapped: {
      auto &oneshot = rt->prebuiltinsts[handler->impl];
      oneshot.preExecPrebuiltMapped();
      break;
    }
#else
    case OpcodeHandler<uint32_t>::OHT_Dynamic: {
      OpcodeHandler<uint32_t> oneshot;
      oneshot.impl = handler->handler();
      oneshot.preExecDynamic();
      break;
    }
#endif
    default:
      abort();
      break;
    }
    auto pc = CPU.pcptr[0];
    while (true) {
      if (rt->contain(pc)) {
        // The current PC is within the same rt binary
        if (rt->rtbase + insn[1].rva == pc) {
          // sequencial instruction, advance to the next one
          insn++;
        } else {
          // non-sequential instruction, likely a branch or jump
          // update the next instruction from the cached index
          insn = mdinsns + rt->nexticache[insn - mdinsns];
          if (rt->rtbase + insn->rva != pc) {
            // find the instruction corresponding to the current PC
            insn = rt->find(pc);
            assert(
                insn &&
                "Failed to find instruction corresponding to the current PC");
            // update the next instruction cache with the newly found
            // instruction
            const_cast<RuntimeMetaData *>(rt)->nexticache[insn - mdinsns] =
                insn - mdinsns;
          }
        }
        break;
      } else if (pc >= rtstart && pc < rtend) {
        // The current PC is within some other rt binary
        rt = find(pc, &insn);
        assert(rt && insn &&
               "Failed to find runtime and instruction for the current PC");
        // update the instruction and metadata pointers for the new runtime
        metadata = rt->metadata;
        mdinsns = metadata->insns();
        operands = metadata->operands();
        expressions = metadata->expressions();
        break;
      } else if (pc == (uint64_t)this) {
        // Finished executing the current function
        return;
      } else {
        // Calling system function
        pc = CallHost(pc);
        continue;
      }
    }
  }
}

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "OpcodeEngine.h"
#include "BinaryEngine.h"
#include "Handler.h"
#include "Lifter.h"
#include "Orchestrator.h"
#include "UtilsAArch64.h"

#include <Platform.h>

#include <llvm/IR/Instruction.h>

// shortcuts for engine implementation stub
#define engine (CPU.runtime)

using RemillRegister = remill::Operand::Register;

namespace aether {

thread_local OpcodeEngine EMU;

namespace {

// Dynamic handler executable page
std::vector<uint64_t> dynhandlers;
uint8_t *pagestart = nullptr, *pagecur = nullptr;

// Shared operand caches
std::vector<Operand> operands;
// <id, ptr> of operand
std::map<uint64_t, const Operand *> operandmap;

// Register offset within the cpu state
std::map<std::string, size_t> regoffs;

void load_regoffs_aarch64(std::map<std::string, size_t> &regoffs) {
  // General Purpose Registers (x0 - x30) and 32-bit aliases (w0 - w30)
  for (int i = 0; i <= 30; ++i) {
    std::string num = std::to_string(i);
    regoffs["x" + num] = aarch64::offset_reg("x" + num);
    regoffs["w" + num] = aarch64::offset_reg("w" + num);
  }

  // Stack Pointer and Program Counter
  regoffs["sp"] = aarch64::offset_reg("sp");
  regoffs["wsp"] = aarch64::offset_reg("wsp");
  regoffs["pc"] = aarch64::offset_reg("pc");

  // SIMD / Vector Registers (v0-v31, q0-q31, d0-d31, s0-s31, h0-h31, b0-b31)
  for (int i = 0; i < 32; ++i) {
    std::string num = std::to_string(i);
    regoffs["v" + num] = aarch64::offset_reg("v" + num);
    regoffs["q" + num] = aarch64::offset_reg("q" + num);
    regoffs["d" + num] = aarch64::offset_reg("d" + num);
    regoffs["s" + num] = aarch64::offset_reg("s" + num);
    regoffs["h" + num] = aarch64::offset_reg("h" + num);
    regoffs["b" + num] = aarch64::offset_reg("b" + num);
  }

  // System Registers
  regoffs["tpidr_el0"] = aarch64::offset_reg("tpidr_el0");
  regoffs["tpidrro_el0"] = aarch64::offset_reg("tpidrro_el0");

  // Condition Flags & Status Registers
  regoffs["nzcv"] = aarch64::offset_reg("nzcv");
  regoffs["fpcr"] = aarch64::offset_reg("fpcr");
  regoffs["fpsr"] = aarch64::offset_reg("fpsr");

  // Individual Condition Flags in SR
  regoffs["n"] = aarch64::offset_reg("n");
  regoffs["z"] = aarch64::offset_reg("z");
  regoffs["c"] = aarch64::offset_reg("c");
  regoffs["v"] = aarch64::offset_reg("v");

  // Individual Cumulative Exception Flags in SR
  regoffs["ixc"] = aarch64::offset_reg("ixc");
  regoffs["ofc"] = aarch64::offset_reg("ofc");
  regoffs["ufc"] = aarch64::offset_reg("ufc");
  regoffs["idc"] = aarch64::offset_reg("idc");
  regoffs["ioc"] = aarch64::offset_reg("ioc");
  regoffs["dzc"] = aarch64::offset_reg("dzc");

  // Sleigh Flag State
  regoffs["ng"] = aarch64::offset_reg("ng");
  regoffs["zr"] = aarch64::offset_reg("zr");
  regoffs["cy"] = aarch64::offset_reg("cy");
  regoffs["ov"] = aarch64::offset_reg("ov");
  regoffs["shift_carry"] = aarch64::offset_reg("shift_carry");
}

void load_regoffs_x64(std::map<std::string, size_t> &regoffs) {
  // Standard Named GPRs and sub-register aliases
  static constexpr std::string_view gpr_names[] = {
      "rax", "eax", "ax",  "al",  "ah",  "rbx", "ebx", "bx",  "bl",  "bh",
      "rcx", "ecx", "cx",  "cl",  "ch",  "rdx", "edx", "dx",  "dl",  "dh",
      "rsi", "esi", "si",  "sil", "rdi", "edi", "di",  "dil", "rsp", "esp",
      "sp",  "spl", "rbp", "ebp", "bp",  "bpl", "rip", "eip", "ip"};

  for (std::string_view name : gpr_names) {
    regoffs[std::string(name)] = x86::offset_reg(name);
  }

  // Numbered GPRs (r8 - r15 and sub-register variants)
  for (int i = 8; i <= 15; ++i) {
    std::string num = std::to_string(i);
    regoffs["r" + num] = x86::offset_reg("r" + num);
    regoffs["r" + num + "d"] = x86::offset_reg("r" + num + "d");
    regoffs["r" + num + "w"] = x86::offset_reg("r" + num + "w");
    regoffs["r" + num + "b"] = x86::offset_reg("r" + num + "b");
  }

  // Vectors (zmm, ymm, xmm)
  for (int i = 0; i < 32; ++i) {
    std::string num = std::to_string(i);
    regoffs["zmm" + num] = x86::offset_reg("zmm" + num);
    regoffs["ymm" + num] = x86::offset_reg("ymm" + num);
    regoffs["xmm" + num] = x86::offset_reg("xmm" + num);
  }

  // AVX-512 Mask / K Registers
  for (int i = 0; i < 8; ++i) {
    std::string reg_name = "k" + std::to_string(i);
    regoffs[reg_name] = x86::offset_reg(reg_name);
  }

  // MMX & x87 Stack Registers
  for (int i = 0; i < 8; ++i) {
    std::string num = std::to_string(i);
    regoffs["mm" + num] = x86::offset_reg("mm" + num);
    regoffs["st" + num] = x86::offset_reg("st" + num);
  }

  // Flags & Segment Registers
  static constexpr std::string_view misc_names[] = {
      "cf",      "pf",      "af",      "zf",      "sf",      "df",
      "of",      "rflags",  "eflags",  "flags",   "ss",      "es",
      "gs",      "fs",      "ds",      "cs",      "ss_base", "es_base",
      "gs_base", "fs_base", "ds_base", "cs_base", "fpu_c0",  "fpu_c1",
      "fpu_c2",  "fpu_c3",  "fpu_pe",  "fpu_ue",  "fpu_oe",  "fpu_ze",
      "fpu_de",  "fpu_ie",  "fpu_sf"};

  for (std::string_view name : misc_names) {
    regoffs[std::string(name)] = x86::offset_reg(name);
  }
}

// Mask off everything above the low `bits` bits of `val` (bits in [0, 64]).
inline uint64_t MaskToBits(uint64_t val, unsigned bits) {
  if (bits == 0) {
    return 0;
  }
  if (bits >= 64) {
    return val;
  }
  return val & ((uint64_t(1) << bits) - uint64_t(1));
}

// Sign-extend the low `bits` bits of `val` out to 64 bits.
inline uint64_t SignExtend(uint64_t val, unsigned bits) {
  if (bits == 0 || bits >= 64) {
    return val;
  }
  val = MaskToBits(val, bits);
  const uint64_t sign_bit = uint64_t(1) << (bits - 1);
  return (val ^ sign_bit) - sign_bit;
}

inline size_t GetOffset(const std::string &reg) {
  auto found = regoffs.find(reg);
  if (found == regoffs.end())
    abort();
  return found->second;
}

// Compute the address of a register within `State`, as a raw integer.
inline uint64_t LoadRegAddressRaw(void *state_ptr, const RemillRegister &reg) {
  return reinterpret_cast<uint64_t>(reinterpret_cast<uint8_t *>(state_ptr) +
                                    GetOffset(reg.name));
}

// Read a register's current value out of `State`, zero-extended to 64 bits.
uint64_t LoadRegValueRaw(void *state_ptr, const RemillRegister &reg) {
  const auto addr =
      reinterpret_cast<const uint8_t *>(state_ptr) + GetOffset(reg.name);
  const size_t num_bytes = (static_cast<size_t>(reg.size) + 7u) / 8u;

  uint64_t val = 0;
  std::memcpy(&val, addr, num_bytes);
  return val;
}

// Return a register's value, or zero if `reg_name` is empty, masked/checked
// against `word_size_bits`.
uint64_t LoadWordRegValOrZeroRaw(void *state_ptr, const RemillRegister &reg,
                                 unsigned word_size_bits) {
  if (reg.name.empty()) {
    return 0;
  }

  // Note: this is a `static` method in the header (matches the original
  // `LoadWordRegValOrZero`), so it can't call the non-static
  // `LoadRegValueRaw`/`impl->arch->RegisterByName` directly here; if you
  // keep it static, either pass in `const Arch *arch` or make it a regular
  // member function. Shown here as a member function for clarity:
  auto val = LoadRegValueRaw(state_ptr, reg);
  return MaskToBits(val, word_size_bits);
}

// Lift a shift-register operand to a raw value.
uint64_t LiftShiftRegisterOperandRaw(void *state_ptr, const RemillOperand &op) {
  auto &arch_reg = op.shift_reg.reg;

  auto reg = LoadRegValueRaw(state_ptr, arch_reg);
  auto reg_size = static_cast<unsigned>(arch_reg.size);
  const auto word_size = sizeof(uintptr_t);

  const uint64_t zero = 0;
  const uint64_t one = 1;
  const uint64_t shift_size = op.shift_reg.shift_size;

  auto curr_size = reg_size;

  using enum RemillOperand::ShiftRegister::Shift;
  using enum RemillOperand::ShiftRegister::Extend;
  if (kExtendInvalid != op.shift_reg.extend_op) {
    const auto extract_size = static_cast<unsigned>(op.shift_reg.extract_size);

    if (reg_size > extract_size) {
      curr_size = extract_size;
      reg = MaskToBits(reg, extract_size);
    }

    if (op.size > op.shift_reg.extract_size) {
      switch (op.shift_reg.extend_op) {
      case kExtendSigned:
        reg = SignExtend(reg, extract_size);
        curr_size = static_cast<unsigned>(op.size);
        break;
      case kExtendUnsigned:
        // Already zero-extended by `MaskToBits`/the 64-bit `uint64_t`
        // representation; nothing further to do beyond bookkeeping.
        curr_size = static_cast<unsigned>(op.size);
        break;
      default:
        abort();
        break;
      }
    }
  }

  if (curr_size < op.size) {
    reg = MaskToBits(reg, curr_size);
    curr_size = static_cast<unsigned>(op.size);
  }

  if (kShiftInvalid != op.shift_reg.shift_op) {
    const auto op_bits = static_cast<unsigned>(op.size);

    switch (op.shift_reg.shift_op) {

    // Left shift.
    case kShiftLeftWithZeroes:
      reg = MaskToBits(reg << shift_size, op_bits);
      break;

    // Masking shift left.
    case kShiftLeftWithOnes: {
      const uint64_t mask_val = ~((~zero) << shift_size);
      reg = MaskToBits((reg << shift_size) | mask_val, op_bits);
      break;
    }

    // Logical right shift.
    case kShiftUnsignedRight:
      reg = MaskToBits(reg >> shift_size, op_bits);
      break;

    // Arithmetic right shift.
    case kShiftSignedRight: {
      const auto signed_reg = static_cast<int64_t>(SignExtend(reg, op_bits));
      reg =
          MaskToBits(static_cast<uint64_t>(signed_reg >> shift_size), op_bits);
      break;
    }

    // Rotate left.
    case kShiftLeftAround: {
      const uint64_t shr_amount = (~shift_size + one) & (op.size - one);
      const auto val1 = reg >> shr_amount;
      const auto val2 = reg << shift_size;
      reg = MaskToBits(val1 | val2, op_bits);
      break;
    }

    // Rotate right.
    case kShiftRightAround: {
      const uint64_t shl_amount = (~shift_size + one) & (op.size - one);
      const auto val1 = reg >> shift_size;
      const auto val2 = reg << shl_amount;
      reg = MaskToBits(val1 | val2, op_bits);
      break;
    }

    case kShiftInvalid:
      break;
    }
  }

  if (word_size > op.size) {
    // Representation is already zero-extended in the `uint64_t`; mask to
    // the operand's own width for a well-defined result.
    reg = MaskToBits(reg, static_cast<unsigned>(op.size));
  }

  return reg;
}

// Lift a register operand to a raw value or address.
//
// For write operands, this returns the register's *address* within `State`
// (as an integer) so the semantics function can store its result there
// directly. For read operands, it returns the register's *value*.
uint64_t LiftRegisterOperandRaw(void *state_ptr, const RemillOperand &op) {
  auto &arch_reg = op.reg;

  if (RemillOperand::kActionWrite == op.action) {
    return LoadRegAddressRaw(state_ptr, arch_reg);
  }

  // NOTE: the original IR-based `LiftRegisterOperand` additionally handled
  // vector-/array-typed registers (e.g. XMM/YMM/ZMM, x87 stack slots) with
  // bitcasts through LLVM's type system, and adjusted width via the
  // semantics function's declared LLVM argument type. In the raw calling
  // convention there is no declared LLVM argument type to consult, so
  // callers are expected to interpret the returned bits according to the
  // operand's own `size`/register type; only up to 64-bit scalar registers
  // are supported directly here (see `LoadRegValueRaw`).
  return LoadRegValueRaw(state_ptr, arch_reg);
}

// Lift an immediate operand to a raw, correctly sign/zero-extended value.
uint64_t LiftImmediateOperandRaw(const RemillOperand &arch_op) {
  const auto bits = static_cast<unsigned>(arch_op.size);
  const auto raw_val = static_cast<uint64_t>(arch_op.imm.val);

  if (arch_op.imm.is_signed) {
    return SignExtend(raw_val, bits);
  }
  return MaskToBits(raw_val, bits);
}

// Compute a memory operand's effective address as a raw value.
uint64_t LiftAddressOperandRaw(void *state_ptr, const RemillOperand &op) {
  auto &arch_addr = op.addr;
  const auto word_size = sizeof(uintptr_t);

  auto addr = LoadWordRegValOrZeroRaw(state_ptr, arch_addr.base_reg, word_size);
  auto index =
      LoadWordRegValOrZeroRaw(state_ptr, arch_addr.index_reg, word_size);
  const auto scale = static_cast<uint64_t>(arch_addr.scale);
  auto segment =
      LoadWordRegValOrZeroRaw(state_ptr, arch_addr.segment_base_reg, word_size);

  if (index) {
    addr = MaskToBits(addr + index * scale, word_size);
  }

  if (arch_addr.displacement) {
    if (0 < arch_addr.displacement) {
      addr = MaskToBits(addr + static_cast<uint64_t>(arch_addr.displacement),
                        word_size);
    } else {
      addr = MaskToBits(addr - static_cast<uint64_t>(-arch_addr.displacement),
                        word_size);
    }
  }

  // Compute the segmented address.
  if (segment) {
    addr = MaskToBits(addr + segment, word_size);
  }

  // Memory address is smaller than the machine word size (e.g. 32-bit address
  // used in 64-bit).
  if (arch_addr.address_size < word_size) {
    addr = MaskToBits(addr, static_cast<unsigned>(arch_addr.address_size));
  }

  return addr;
}

// Recursively evaluate an operand expression tree to a raw value.
//
// NOTE: assumes `OperandExpression`/`LLVMOpExpr` have been adjusted so that
// `op->type`-like width information is a plain `unsigned bit_width` and any
// embedded constant is a raw `uint64_t` rather than `llvm::Type *` /
// `llvm::Constant *`. Substitute the correct accessor names below if your
// header uses different ones.
uint64_t LiftExpressionOperandRecRaw(void *state_ptr,
                                     const remill::OperandExpression *op) {
  if (auto llvm_op = std::get_if<remill::LLVMOpExpr>(op)) {
    auto lhs = LiftExpressionOperandRecRaw(state_ptr, llvm_op->op1);
    uint64_t rhs = 0;
    if (llvm_op->op2) {
      rhs = LiftExpressionOperandRecRaw(state_ptr, llvm_op->op2);
    }

    switch (llvm_op->llvm_opcode) {
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
      return MaskToBits(
          lhs,
          engine->remillSemantic->getDataLayout().getTypeSizeInBits(op->type));
    case llvm::Instruction::SExt:
      return SignExtend(
          lhs,
          engine->remillSemantic->getDataLayout().getTypeSizeInBits(op->type));
    case llvm::Instruction::Trunc:
      return MaskToBits(
          lhs,
          engine->remillSemantic->getDataLayout().getTypeSizeInBits(op->type));
    case llvm::Instruction::And:
      return lhs & rhs;
    case llvm::Instruction::Or:
      return lhs | rhs;
    case llvm::Instruction::URem:
      return rhs ? (lhs % rhs) : 0;
    case llvm::Instruction::Xor:
      return lhs ^ rhs;
    default:
      abort();
      return 0;
    }
  } else if (auto reg_op = std::get_if<const remill::Register *>(op)) {
    RemillRegister reg;
    reg.name = (*reg_op)->name;
    reg.size = (*reg_op)->size;
    return LoadRegValueRaw(state_ptr, reg);
  } else if (auto ci_op = std::get_if<llvm::Constant *>(op)) {
    if (const auto *ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(*ci_op))
      return ci->getZExtValue();

    abort();
  } else if (auto str_op = std::get_if<std::string>(op)) {
    RemillRegister reg;
    reg.name = *str_op;
    reg.size = str_op->at(0) == 'w' ? 32 : 64;
    return LoadRegValueRaw(state_ptr, reg);
  } else {
    abort();
  }
}

// Lift an expression operand to a raw value.
uint64_t LiftExpressionOperandRaw(void *state_ptr, const RemillOperand &op) {
  // The IR path additionally zext/trunc'd the expression's result to match
  // the semantics function's declared LLVM argument type. There's no such
  // declared type on the raw path, so the caller is expected to know the
  // operand's width from `op.size`; callers needing a specific width can
  // mask/sign-extend the result with `op.size` themselves.
  return LiftExpressionOperandRecRaw(state_ptr, op.expr);
}

inline bool IsARM64() {
  return engine->remillArch->arch_name == remill::kArchAArch64LittleEndian;
}

inline void *GetState() {
  return IsARM64() ? (void *)&CPU.aarch64 : (void *)&CPU.x86;
}

} // namespace

uint64_t RemillOperand::ID() const {
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
  return id | (((uint64_t)type) << 60) |
         (((uint64_t)(action == kActionWrite)) << 60);
}

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
  if (!regoffs.size()) {
    if (IsARM64())
      load_regoffs_aarch64(regoffs);
    else
      load_regoffs_x64(regoffs);
  }
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
  auto state = GetState();
  for (auto &op : inst.operands) {
    auto optr = (RemillOperand *)&op;
    auto id = optr->ID();
    auto found = operandmap.find(id);
    if (found == operandmap.end()) {
      uintptr_t val = 0;
      switch (op.type) {
      case RemillOperand::kTypeRegister:
        val = LiftRegisterOperandRaw(state, *optr);
        break;
      case RemillOperand::kTypeImmediate:
        val = LiftImmediateOperandRaw(*optr);
        break;
      default:
        break;
      }
      operands.push_back({*optr, val});
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

template <> void OpcodeHandler<uint8_t>::initPrebuilt() { abort(); }
template <> void OpcodeHandler<uint16_t>::initPrebuilt() { abort(); }
template <> void OpcodeHandler<uint64_t>::initPrebuilt() { abort(); }
template <> void OpcodeHandler<uint128_var_t>::initPrebuilt() { abort(); }

template <typename T> bool OpcodeHandler<T>::interpRemill() const {
  uint64_t params[16];
  auto state = GetState();
  params[0] = 0;               // memory
  params[1] = (uint64_t)state; // cpu state
  auto i = 2;
  for (auto &opv : args) {
    if (opv->val) {
      params[i++] = opv->val;
      continue;
    }
    switch (opv->op.type) {
    case RemillOperand::kTypeShiftRegister:
      params[i++] = LiftShiftRegisterOperandRaw(state, opv->op);
      break;
    case RemillOperand::kTypeAddress:
      params[i++] = LiftAddressOperandRaw(state, opv->op);
      break;
    case RemillOperand::kTypeExpression:
    case RemillOperand::kTypeRegisterExpression:
    case RemillOperand::kTypeImmediateExpression:
    case RemillOperand::kTypeAddressExpression:
      params[i++] = LiftExpressionOperandRaw(state, opv->op);
      break;
    default:
      return false;
    }
  }

#define CASE_N(N, ...)                                                         \
  case N:                                                                      \
    ((void (*)(__VA_ARGS__))impl)(PARAMS_##N);                                 \
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
    return false;
  }
  return true;
}

static void *vm_retaddr() {
  // reused as a temporary return address
  return &CPU.rvalue;
}

static inline void vm_entry(const Instruction *insns) {
#if AETHER_ARCH_ARM64
  aarch64::aether_vm_entry(GetState(), *CPU.pcptr, insns, &CPU.retaddr,
                           vm_retaddr);
#else
  x86::aether_vm_entry(GetState(), *CPU.pcptr, insns, &CPU.retaddr, vm_retaddr);
#endif
}

template <typename T> void OpcodeHandler<T>::execDynamic() const {
  Instruction insns[2]{{(event_func_t)impl}, {finish_emulation}};
  vm_entry(&insns[0]);
}

template <typename T> void OpcodeHandler<T>::execPrebuilt() const {
#if AETHER_OS_DARWIN_IOS
  vm_entry((Instruction *)&chains[0]);
#else
  abort();
#endif
}

template <typename T> void OpcodeHandler<T>::execPrebuiltMapped() const {
#if AETHER_OS_DARWIN_IOS
  RegisterValue gprmapper[8];
  RegisterValueSIMD fpumapper[8];
  // save mapper register and then set the mapper value to mappee
  int i = 0;
  for (auto &r : gpr) {
    gprmapper[i++] = *CPU.getRegisterAArch64(r.first);
    CPU.setRegisterAArch64(r.first, *CPU.getRegisterAArch64(r.second));
  }
  i = 0;
  for (auto &r : fpu) {
    fpumapper[i++] = *(RegisterValueSIMD *)CPU.getRegisterAArch64(r.first);
    CPU.setRegisterNEON(r.first,
                        *(RegisterValueSIMD *)CPU.getRegisterAArch64(r.second));
  }
  execPrebuilt();
  // load mapper register's original value
  i = 0;
  for (auto &r : gpr) {
    CPU.setRegisterAArch64(r.first, gprmapper[i++]);
  }
  i = 0;
  for (auto &r : fpu) {
    CPU.setRegisterNEON(r.first, fpumapper[i++]);
  }
#else
  abort();
#endif
}

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
  auto arm64 = arch->arch_name == remill::kArchAArch64LittleEndian;
  remill::Instruction inst;
  while (ptr < endptr) {
    std::ignore = arch->DecodeInstruction(0, {ptr, endptr}, inst,
                                          arch->CreateInitialContext());
    if (inst.bytes.size() == 0) {
      ptr += arm64 ? 4 : 1;
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

OpcodeEngine::~OpcodeEngine() {
  if (!readonly) {
    // child thread exiting
    return;
  }

  // binary engine exiting, clear all the caches

  for (auto page : dynhandlers)
    page_dealloc(page, page_size());

  pagestart = nullptr;
  pagecur = nullptr;
  dynhandlers.clear();
  operands.clear();
  operandmap.clear();
  regoffs.clear();
}

} // namespace aether

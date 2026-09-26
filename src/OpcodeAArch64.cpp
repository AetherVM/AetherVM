// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "BinaryEngine.h"
#include "CPUState.h"
#include "OpcodeEngine.h"

#include <Utils.h>

#include <llvm/MC/MCInst.h>

#define GET_REGINFO_ENUM
#define GET_INSTRINFO_ENUM
#include <Target/AArch64/AArch64GenInstrInfo.inc>
#include <Target/AArch64/AArch64GenRegisterInfo.inc>

using namespace llvm;

namespace aether {

namespace {

// Shared MCInst caches
std::vector<llvm::MCInst> insts;

// <opcode, index> of inst
std::map<uint32_t, uint32_t> instmap;

inline void interp_STPDpre(llvm::MCInst &inst) {
  // <MCInst 7577
  //  0: <MCOperand Reg:8>
  //  1: <MCOperand Reg:57>
  //  2: <MCOperand Reg:56>
  //  3: <MCOperand Reg:8>
  //  4: <MCOperand Imm:-14>>
  //  I: stp dm, dn, [sp, #imm]!
  auto idm = inst.getOperand(1).getReg() - AArch64::D0;
  auto idn = inst.getOperand(2).getReg() - AArch64::D0;
  auto imm = inst.getOperand(4).getImm() * 8;
  auto dm = CPU.getRegisterAArch64((Register)((int)Register::Q0 + idm));
  auto dn = CPU.getRegisterAArch64((Register)((int)Register::Q0 + idn));
  auto sp = CPU.getRegisterAArch64(Register::SP)->s8;
  sp += (int64_t)imm;
  std::memcpy((void *)(sp + 0), dm, 8);
  std::memcpy((void *)(sp + 8), dn, 8);
  CPU.setRegisterAArch64(Register::SP, {.s8 = sp});
}

inline void interp_STRDpre(llvm::MCInst &inst) {
  // <MCInst 7601
  //  0: <MCOperand Reg:8>
  //  1: <MCOperand Reg:56>
  //  2: <MCOperand Reg:8>
  //  3: <MCOperand Imm:-96>>
  //  I: str d8, [sp, #-0x60]!
  auto idm = inst.getOperand(1).getReg() - AArch64::D0;
  auto imm = inst.getOperand(3).getImm();
  auto dm = CPU.getRegisterAArch64((Register)((int)Register::Q0 + idm));
  auto sp = CPU.getRegisterAArch64(Register::SP)->s8;
  sp += (int64_t)imm;
  std::memcpy((void *)sp, dm, 8);
  CPU.setRegisterAArch64(Register::SP, {.s8 = sp});
}

inline void interp_STPQi(llvm::MCInst &inst) {
  // <MCInst 7578
  //  0: <MCOperand Reg:144>
  //  1: <MCOperand Reg:144>
  //  2: <MCOperand Reg:8>
  //  3: <MCOperand Imm:12>>
  //  I: stp qm, qn, [sp, #imm]
  auto iqm = inst.getOperand(0).getReg() - AArch64::Q0;
  auto iqn = inst.getOperand(1).getReg() - AArch64::Q0;
  auto imm = inst.getOperand(3).getImm() * 16;
  auto qm = CPU.getRegisterAArch64((Register)((int)Register::Q0 + iqm));
  auto qn = CPU.getRegisterAArch64((Register)((int)Register::Q0 + iqn));
  auto sp = CPU.getRegisterAArch64(Register::SP)->s8;
  sp += (int64_t)imm;
  std::memcpy((void *)(sp + 0), qm, 16);
  std::memcpy((void *)(sp + 8), qn, 16);
}

} // namespace

template <typename T>
bool OpcodeHandler<T>::initMCInstARM64(llvm::MCInst &inst) {
  for (unsigned i = 0; i < inst.getNumOperands(); i++) {
    auto &operand = inst.getOperand(i);
    if (operand.isReg() && operand.getReg() == AArch64::SP) {
      type = OHT_MCInst;

      auto found = instmap.find(*(uint32_t *)&opcode);
      if (found == instmap.end()) {
        insts.push_back(inst);
        found = instmap
                    .insert(std::make_pair(*(uint32_t *)&opcode,
                                           (int)insts.size() - 1))
                    .first;
      }
      args.push_back(OperandInfo{found->second, 0});
      return true;
    }
  }
  return false;
}

#define case_interp(I)                                                         \
  case AArch64::I:                                                             \
    interp_##I(inst);                                                          \
    break;

template <typename T> void OpcodeHandler<T>::interpMCInstARM64() const {
  auto &inst = insts[args[0].index];
  switch (inst.getOpcode()) {
    case_interp(STPDpre);
    case_interp(STRDpre);
    case_interp(STPQi);
  default: {
    std::string strinst;
    CPU.runtime->diser.print(inst, strinst);
    log_print(Runtime, "Unimplemented aarch64 opcode at {:p}:{:x} {}.",
              (void *)&opcode, *(uint32_t *)&opcode, strinst);
    inst.print(llvm::errs());
    putchar('\n');
    exit(-1);
  }
  }
  // update pc to the next address
  auto pc = CPU.getRegisterAArch64(Register::PC)->u8;
  CPU.setRegisterAArch64(Register::PC, {.u8 = pc + 4});
}

template struct OpcodeHandler<uint8_t>;
template struct OpcodeHandler<uint16_t>;
template struct OpcodeHandler<uint32_t>;
template struct OpcodeHandler<uint64_t>;
template struct OpcodeHandler<uint128_var_t>;

} // namespace aether

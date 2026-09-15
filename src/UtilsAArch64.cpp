// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "UtilsAArch64.h"
#include "BinaryEngine.h"
#include "Lifter.h"

#include <Disassembler.h>
#include <Utils.h>
#include <fstream>

#include <llvm/MC/MCInst.h>

#define GET_REGINFO_ENUM
#define GET_INSTRINFO_ENUM
#include <Target/AArch64/AArch64GenInstrInfo.inc>
#include <Target/AArch64/AArch64GenRegisterInfo.inc>

namespace aether {

namespace aarch64 {

std::set<aether::Register> parse_regused(const llvm::MCInst &inst) {
  using namespace llvm;
  std::set<uint8_t> xregs, qregs;
  for (unsigned i = 0; i < inst.getNumOperands(); i++) {
    auto opr = inst.getOperand(i);
    if (!opr.isReg())
      continue;
    auto reg = opr.getReg();
    if (reg == AArch64::WZR || reg == AArch64::XZR)
      continue;
    if (reg >= AArch64::W0 && reg <= AArch64::W30) {
      xregs.insert(reg - AArch64::W0);
    } else if (reg == AArch64::WSP) {
      xregs.insert(31);
    } else if (reg >= AArch64::W0_W1 && reg <= AArch64::W28_W29) {
      xregs.insert(reg - AArch64::W0_W1 + 0);
      xregs.insert(reg - AArch64::W0_W1 + 1);
    } else if (reg >= AArch64::X0 && reg <= AArch64::X28) {
      xregs.insert(reg - AArch64::X0);
    } else if (reg >= AArch64::X0_X1 && reg <= AArch64::X26_X27) {
      xregs.insert(reg - AArch64::X0_X1 + 0);
      xregs.insert(reg - AArch64::X0_X1 + 1);
    } else if (reg == AArch64::FP) {
      xregs.insert(29);
    } else if (reg == AArch64::LR) {
      xregs.insert(30);
    } else if (reg == AArch64::SP) {
      xregs.insert(31);
    } else if (reg >= AArch64::B0 && reg <= AArch64::B31) {
      qregs.insert(reg - AArch64::B0);
    } else if (reg >= AArch64::H0 && reg <= AArch64::H31) {
      qregs.insert(reg - AArch64::H0);
    } else if (reg >= AArch64::S0 && reg <= AArch64::S31) {
      qregs.insert(reg - AArch64::S0);
    } else if (reg >= AArch64::D0 && reg <= AArch64::D31) {
      qregs.insert(reg - AArch64::D0);
    } else if (reg >= AArch64::D0_D1 && reg <= AArch64::D30_D31) {
      qregs.insert(reg - AArch64::D0_D1 + 0);
      qregs.insert(reg - AArch64::D0_D1 + 1);
    } else if (reg >= AArch64::D0_D1_D2 && reg <= AArch64::D29_D30_D31) {
      qregs.insert(reg - AArch64::D0_D1_D2 + 0);
      qregs.insert(reg - AArch64::D0_D1_D2 + 1);
      qregs.insert(reg - AArch64::D0_D1_D2 + 2);
    } else if (reg >= AArch64::D0_D1_D2_D3 && reg <= AArch64::D28_D29_D30_D31) {
      qregs.insert(reg - AArch64::D0_D1_D2_D3 + 0);
      qregs.insert(reg - AArch64::D0_D1_D2_D3 + 1);
      qregs.insert(reg - AArch64::D0_D1_D2_D3 + 2);
      qregs.insert(reg - AArch64::D0_D1_D2_D3 + 3);
    } else if (reg >= AArch64::Q0 && reg <= AArch64::Q31) {
      qregs.insert(reg - AArch64::Q0);
    } else if (reg >= AArch64::Q0_Q1 && reg <= AArch64::Q30_Q31) {
      qregs.insert(reg - AArch64::Q0_Q1 + 0);
      qregs.insert(reg - AArch64::Q0_Q1 + 1);
    } else if (reg >= AArch64::Q0_Q1_Q2 && reg <= AArch64::Q29_Q30_Q31) {
      qregs.insert(reg - AArch64::Q0_Q1_Q2 + 0);
      qregs.insert(reg - AArch64::Q0_Q1_Q2 + 1);
      qregs.insert(reg - AArch64::Q0_Q1_Q2 + 2);
    } else if (reg >= AArch64::Q0_Q1_Q2_Q3 && reg <= AArch64::Q28_Q29_Q30_Q31) {
      qregs.insert(reg - AArch64::Q0_Q1_Q2_Q3 + 0);
      qregs.insert(reg - AArch64::Q0_Q1_Q2_Q3 + 1);
      qregs.insert(reg - AArch64::Q0_Q1_Q2_Q3 + 2);
      qregs.insert(reg - AArch64::Q0_Q1_Q2_Q3 + 3);
    } else if (reg >= AArch64::Z0 && reg <= AArch64::Z31) {
      qregs.insert(reg - AArch64::Z0);
    } else if (reg >= AArch64::Z0_Z1 && reg <= AArch64::Z30_Z31) {
      qregs.insert(reg - AArch64::Z0_Z1 + 0);
      qregs.insert(reg - AArch64::Z0_Z1 + 1);
    } else if (reg >= AArch64::Z0_Z1_Z2 && reg <= AArch64::Z29_Z30_Z31) {
      qregs.insert(reg - AArch64::Z0_Z1_Z2 + 0);
      qregs.insert(reg - AArch64::Z0_Z1_Z2 + 1);
      qregs.insert(reg - AArch64::Z0_Z1_Z2 + 2);
    } else if (reg >= AArch64::Z0_Z1_Z2_Z3 && reg <= AArch64::Z28_Z29_Z30_Z31) {
      qregs.insert(reg - AArch64::Z0_Z1_Z2_Z3 + 0);
      qregs.insert(reg - AArch64::Z0_Z1_Z2_Z3 + 1);
      qregs.insert(reg - AArch64::Z0_Z1_Z2_Z3 + 2);
      qregs.insert(reg - AArch64::Z0_Z1_Z2_Z3 + 3);
    }
  }
  std::set<aether::Register> regused;
  for (auto x : xregs)
    regused.insert((aether::Register)((uint8_t)Register::X0 + x));
  for (auto q : qregs)
    regused.insert((aether::Register)((uint8_t)Register::Q0 + q));
  return regused;
}

uint32_t normalize_opcode(Disassembler &diser, llvm::MCInst &inst,
                          uint32_t opcode, aarch64::OpcodeRegisters &opregs) {
  using namespace llvm;
  std::set<unsigned> &asmerropcs = opregs.asmerropcs;
  if (asmerropcs.find(inst.getOpcode()) != asmerropcs.end())
    return opcode;

  std::set<unsigned> &regused = opregs.regused, &fpuused = opregs.fpuused;
  for (unsigned i = 0; i < inst.getNumOperands(); i++) {
    auto opr = inst.getOperand(i);
    if (!opr.isReg())
      continue;
    auto reg = opr.getReg();
    if (reg >= AArch64::W0 && reg <= AArch64::W28)
      regused.insert(reg - AArch64::W0);
    else if (reg >= AArch64::X0 && reg <= AArch64::X28)
      regused.insert(reg - AArch64::X0);
    else if (reg >= AArch64::B0 && reg <= AArch64::B31)
      fpuused.insert(reg - AArch64::B0);
    else if (reg >= AArch64::H0 && reg <= AArch64::H31)
      fpuused.insert(reg - AArch64::H0);
    else if (reg >= AArch64::S0 && reg <= AArch64::S31)
      fpuused.insert(reg - AArch64::S0);
    else if (reg >= AArch64::D0 && reg <= AArch64::D31)
      fpuused.insert(reg - AArch64::D0);
    else if (reg >= AArch64::Q0 && reg <= AArch64::Q31)
      fpuused.insert(reg - AArch64::Q0);
    else if (reg >= AArch64::Z0 && reg <= AArch64::Z31)
      fpuused.insert(reg - AArch64::Z0);
  }
  if (*fpuused.rbegin() < 16)
    return opcode;

  std::map<unsigned, unsigned> &regmaps = opregs.regmaps,
                               &fpumaps = opregs.fpumaps;
  unsigned ireg = 0;
  for (auto i : regused)
    regmaps.insert({i, ireg++});
  ireg = 0;
  for (auto i : fpuused)
    fpumaps.insert({i, ireg++});

  for (unsigned i = 0; i < inst.getNumOperands(); i++) {
    auto &opr = inst.getOperand(i);
    if (!opr.isReg())
      continue;
    auto reg = opr.getReg();
    if (reg >= AArch64::W0 && reg <= AArch64::W28)
      opr.setReg(regmaps.find(reg - AArch64::W0)->second + AArch64::W0);
    else if (reg >= AArch64::X0 && reg <= AArch64::X28)
      opr.setReg(regmaps.find(reg - AArch64::X0)->second + AArch64::X0);
    else if (reg >= AArch64::B0 && reg <= AArch64::B31)
      opr.setReg(fpumaps.find(reg - AArch64::B0)->second + AArch64::B0);
    else if (reg >= AArch64::H0 && reg <= AArch64::H31)
      opr.setReg(fpumaps.find(reg - AArch64::H0)->second + AArch64::H0);
    else if (reg >= AArch64::S0 && reg <= AArch64::S31)
      opr.setReg(fpumaps.find(reg - AArch64::S0)->second + AArch64::S0);
    else if (reg >= AArch64::D0 && reg <= AArch64::D31)
      opr.setReg(fpumaps.find(reg - AArch64::D0)->second + AArch64::D0);
    else if (reg >= AArch64::Q0 && reg <= AArch64::Q31)
      opr.setReg(fpumaps.find(reg - AArch64::Q0)->second + AArch64::Q0);
    else if (reg >= AArch64::Z0 && reg <= AArch64::Z31)
      opr.setReg(fpumaps.find(reg - AArch64::Z0)->second + AArch64::Z0);
  }

  std::string strinst;
  uint8_t newopcode[20] = {0};
  diser.print(inst, strinst);
  diser.assemble(strinst.data(), newopcode, false);
  if (newopcode[0] != 4) {
    asmerropcs.insert(inst.getOpcode());
    return opcode;
  }
  return *(uint32_t *)&newopcode[1];
}

} // namespace aarch64

size_t opcode_generator(std::string_view path) {
  EventConfig conf;
  BinaryEngineImpl engine{ARM64, MachO, conf, nullptr};
  Lifter lifter{&engine, nullptr,
                const_cast<remill::Arch *>(engine.remillArch.get()),
                engine.remillSemantic.get()};
  Disassembler diser("arm64");
  llvm::MCInst inst;
  std::set<uint32_t> opcodes;
  std::set<unsigned> canopc, cannot;
  aarch64::OpcodeRegisters opregs;
  uint64_t progress = -1, min = 0, max = 0xFFFFFFFF;
  for (uint64_t opcode = min; opcode <= max; opcode++) {
    auto prog = (opcode - min) * 100 / (max - min);
    if (prog != progress) {
      progress = prog;
      log_print(Runtime, "Iterating arm64 instruction set {}%...", progress);
    }

    if (diser.disassemble((uint32_t)opcode, inst) != 4)
      continue;

    auto opc = inst.getOpcode();
    if (opc <= llvm::AArch64::ABS_ZPmZ_B_UNDEF || opc == llvm::AArch64::UDF)
      continue;

    if (canopc.find(opc) != canopc.end())
      continue;

    auto regused = aarch64::parse_regused(inst);
    bool x18293031 = false, neon = false;
    for (auto r : regused) {
      switch (r) {
      case Register::X18:
      case Register::X29:
      case Register::X30:
      case Register::X31:
        x18293031 = true;
        break;
      default:
        if (!neon)
          neon = Register::Q0 <= r && r <= Register::Q31;
        break;
      }
    }
    if (x18293031 || !neon)
      continue;

    if (cannot.find(opc) != cannot.end()) {
      opcodes.insert(normalize_opcode(diser, inst, opcode, opregs));
      continue;
    }

    if (lifter.canLift({(const uint8_t *)&opcode, 4})) {
      canopc.insert(opc);
      continue;
    }

    cannot.insert(opc);
    opcodes.insert(normalize_opcode(diser, inst, opcode, opregs));
  }

  std::ofstream outf{path.data(), std::ios::binary};
  for (auto opcode : opcodes)
    outf.write((char *)&opcode, sizeof(opcode));

  log_print(Runtime, "Created {}.", path);
  return opcodes.size();
}

size_t opcret_generator(std::string_view outpath) {
  constexpr uint32_t insn_ret = 0xD65F03C0;
  // remove .ret suffix
  auto inpath = std::string{outpath.data(), outpath.size() - 4};
  std::ifstream inf{inpath.data(), std::ios::binary};
  std::ofstream outf{outpath.data(), std::ios::binary};
  size_t i = 0;
  while (!inf.eof()) {
    // add a ret instruction for each opcode
    uint32_t opc[2]{0, insn_ret};
    inf.read((char *)&opc[0], 4);
    outf.write((char *)&opc[0], 8);
    i++;
  }
  log_print(Runtime, "Created {}.", outpath);
  return i;
}

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "UtilsAArch64.h"
#include "BinaryEngine.h"
#include "Lifter.h"

#include <Disassembler.h>
#include <Utils.h>

#include <flat_set>
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
    if (reg >= AArch64::W0 && reg <= AArch64::W30)
      regused.insert(reg - AArch64::W0);
    else if (reg >= AArch64::X0 && reg <= AArch64::X28)
      regused.insert(reg - AArch64::X0);
    else if (reg == AArch64::FP)
      regused.insert(29);
    else if (reg == AArch64::LR)
      regused.insert(30);
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
  if (*fpuused.rbegin() < 8)
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
    if (reg >= AArch64::W0 && reg <= AArch64::W30)
      opr.setReg(regmaps.find(reg - AArch64::W0)->second + AArch64::W0);
    else if (reg >= AArch64::X0 && reg <= AArch64::X28)
      opr.setReg(regmaps.find(reg - AArch64::X0)->second + AArch64::X0);
    else if (reg == AArch64::FP)
      opr.setReg(regmaps.find(29)->second + AArch64::X0);
    else if (reg == AArch64::LR)
      opr.setReg(regmaps.find(30)->second + AArch64::X0);
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
  std::flat_set<uint32_t> opcodes;
  std::flat_set<uint16_t> canopc, cannot;
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
    // x18 is reserved on iOS
    // x31 is sp which will be interpreted by MCInst interpreter of OpcodeEngine
    bool x1831 = false, neon = false;
    for (auto r : regused) {
      switch (r) {
      case Register::X18:
      case Register::X31:
        x1831 = true;
        break;
      default:
        if (!neon)
          neon = Register::Q0 <= r && r <= Register::Q31;
        break;
      }
    }
    if (x1831 || !neon)
      continue;

    aarch64::OpcodeRegisters opregs;
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
    if (inf.gcount() != 4)
      break;
    outf.write((char *)&opc[0], 8);
    i++;
  }
  log_print(Runtime, "Created {}.", outpath);

  auto outdir = fs::path(outpath).parent_path();
  auto ctxswitchsrc = outdir / "ContextSwitch.cpp";
  std::ofstream outsrc{ctxswitchsrc};
  outsrc << R"(// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Orchestrator.h>

)";
  constexpr int max_gpr = 31, max_fpu = 32;
  std::array<int, 7> cpu_regs{0, 1, 2, 3, 4, 5, 26};
  for (int x = 0; x < max_gpr; x++) {
    for (auto c : cpu_regs) {
      outsrc << std::format(
          R"(AETHER_NAKED void vm_opcode_chain_v2h_gpr{0}_cpu{1}(void) {{
  AETHER_ASM("ldr x{0}, [x{1}, #{2:#x}]\n"
    "" extract_handler_x30_pre ""
    "br x30"
  );
}}

AETHER_NAKED void vm_opcode_chain_h2v_gpr{0}_cpu{1}(void) {{
  AETHER_ASM("str x{0}, [x{1}, #{2:#x}]\n"
    "" extract_handler_x30_pre ""
    "br x30"
  );
}}
  
)",
          x, c, aarch64::offset_reg((Register)((int)Register::X0 + x)));
    }
  }
  for (int q = 0; q < max_fpu; q++) {
    for (auto c : cpu_regs) {
      outsrc << std::format(
          R"(AETHER_NAKED void vm_opcode_chain_v2h_fpu{0}_cpu{1}(void) {{
  AETHER_ASM("ldr q{0}, [x{1}, #{2:#x}]\n"
    "" extract_handler_x30_pre ""
    "br x30"
  );
}}

AETHER_NAKED void vm_opcode_chain_h2v_fpu{0}_cpu{1}(void) {{
  AETHER_ASM("str q{0}, [x{1}, #{2:#x}]\n"
    "" extract_handler_x30_pre ""
    "br x30"
  );
}}
  
)",
          q, c, aarch64::offset_reg((Register)((int)Register::Q0 + q)));
    }
  }
  for (int x = 0; x < max_gpr; x++) {
    outsrc << std::format("const void *vm_opcode_chain_v2h_gpr{}[] = {{\n", x);
    for (auto c : cpu_regs)
      outsrc << std::format("\t(void *)&vm_opcode_chain_v2h_gpr{}_cpu{},\n", x,
                            c);
    outsrc << "};\n\n";
  }
  for (int x = 0; x < max_gpr; x++) {
    outsrc << std::format("const void *vm_opcode_chain_h2v_gpr{}[] = {{\n", x);
    for (auto c : cpu_regs)
      outsrc << std::format("\t(void *)&vm_opcode_chain_h2v_gpr{}_cpu{},\n", x,
                            c);
    outsrc << "};\n\n";
  }
  outsrc << std::format("const void *vm_opcode_chain_v2h_xs[] = {{\n");
  for (int x = 0; x < max_gpr; x++) {
    outsrc << std::format("\t&vm_opcode_chain_v2h_gpr{}[0],\n", x);
  }
  outsrc << "};\n\n";
  outsrc << std::format("const void *vm_opcode_chain_h2v_xs[] = {{\n");
  for (int x = 0; x < max_gpr; x++) {
    outsrc << std::format("\t&vm_opcode_chain_h2v_gpr{}[0],\n", x);
  }
  outsrc << "};\n\n";
  for (int x = 0; x < max_fpu; x++) {
    outsrc << std::format("const void *vm_opcode_chain_v2h_fpu{}[] = {{\n", x);
    for (auto c : cpu_regs)
      outsrc << std::format("\t(void *)&vm_opcode_chain_v2h_fpu{}_cpu{},\n", x,
                            c);
    outsrc << "};\n\n";
  }
  for (int x = 0; x < max_fpu; x++) {
    outsrc << std::format("const void *vm_opcode_chain_h2v_fpu{}[] = {{\n", x);
    for (auto c : cpu_regs)
      outsrc << std::format("\t(void *)&vm_opcode_chain_h2v_fpu{}_cpu{},\n", x,
                            c);
    outsrc << "};\n\n";
  }
  outsrc << std::format("const void *vm_opcode_chain_v2h_qs[] = {{\n");
  for (int x = 0; x < max_fpu; x++) {
    outsrc << std::format("\t&vm_opcode_chain_v2h_fpu{}[0],\n", x);
  }
  outsrc << "};\n\n";
  outsrc << std::format("const void *vm_opcode_chain_h2v_qs[] = {{\n");
  for (int x = 0; x < max_fpu; x++) {
    outsrc << std::format("\t&vm_opcode_chain_h2v_fpu{}[0],\n", x);
  }
  outsrc << "};\n\n";

  return i;
}

} // namespace aether

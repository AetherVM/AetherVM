// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <Register.h>

#include <map>
#include <set>

namespace llvm {
class MCInst;
}

namespace aether {

class Disassembler;

namespace aarch64 {

struct OpcodeRegisters {
  std::set<unsigned> regused, fpuused;
  std::map<unsigned, unsigned> regmaps, fpumaps;
  std::set<unsigned> asmerropcs;
};

std::set<aether::Register> parse_regused(const llvm::MCInst &inst);

uint32_t normalize_opcode(Disassembler &diser, llvm::MCInst &inst,
                          uint32_t opcode, OpcodeRegisters &opregs);
} // namespace aarch64

} // namespace aether

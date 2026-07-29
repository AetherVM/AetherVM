// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <AetherBinary.h>

namespace aether {

// an executable instruction
struct Instruction {
  // the handler of this instruction
  uintptr_t handler : 60;
  // the original raw opcode length for x86_64
  // it's always 0 for arm64
  uintptr_t oplen : 4;
};

using Instructions = std::vector<Instruction>;

// an executable basic block
struct BasicBlock {
  // the guest vm address
  addr_t vmaddr;
  // the dynamic handlers represent original instructions in this block
  Instructions handlers;

  bool operator<(const BasicBlock &rhs) const { return vmaddr < rhs.vmaddr; }
  bool operator==(const BasicBlock &rhs) const { return vmaddr == rhs.vmaddr; }
};

// the executable representation of an AetherBinary instance
using BlockChain = std::vector<BasicBlock>;

// chains for multiple binaries
using BlockChains = std::vector<BlockChain>;

class Orchestrator {
public:
  // the current used chain cache
  BlockChain *current = nullptr;
  // all the encoded chains
  BlockChains chains;

public:
  static Orchestrator *inst() {
    static Orchestrator single;
    return &single;
  }

  // encode the bin into a BlockChain
  void encode(const Binary *bin, addr_t addend);

  // find the target Instruction of the given vmaddr
  const Instruction *find(addr_t vmaddr);

private:
  // returned if find with an invalid vmaddr
  Instruction terminate;

  Orchestrator();
  ~Orchestrator() {}
};

void terminate_execution();

} // namespace aether

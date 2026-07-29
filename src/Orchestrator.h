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

// instruction sequence
using Instructions = std::vector<Instruction>;

// the executable representation of an AetherBinary instance
struct BlockChain {
  // the guest basic block vm address
  std::vector<addr_t> vmaddrs;
  // the dynamic handlers represent original instructions in those blocks
  std::vector<Instructions> handlers;
};

// chains for multiple binaries
using BlockChains = std::vector<BlockChain>;

class Orchestrator {
public:
  struct Cache {
    // the current used chain cache
    const BlockChain *current;
    // for fast lookup
    struct {
      addr_t vmaddr;
      const Instruction *insn;
    } L1[256];
  };

  // the cache
  static thread_local Cache cache;

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
  const Instruction *find(addr_t vmaddr) const;

private:
  // returned if find with an invalid vmaddr
  Instruction terminate;

  Orchestrator();
  ~Orchestrator() {}

  static const Instruction *findCache(addr_t vmaddr);
  static const Instruction *findChain(const BlockChain &chain, addr_t vmaddr);

  Orchestrator(const Orchestrator &) = delete;
  Orchestrator &operator=(const Orchestrator &) = delete;
};

void terminate_execution();

} // namespace aether

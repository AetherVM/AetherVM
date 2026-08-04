// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <AetherBinary.h>
#include <Event.h>

namespace aether {

using event_func_t = void (*)(void);

// an executable instruction
struct Instruction {
  // the handler of this instruction
  uintptr_t handler : 59;
  // insn/block/func before/after event flag
  uintptr_t event : 1;
  // the original raw opcode length
  // 0 for event handlers
  uintptr_t oplen : 4;

  Instruction(event_func_t func) {
    handler = reinterpret_cast<uintptr_t>(func);
    event = true;
    oplen = 0;
  }

  Instruction(uintptr_t entry, uintptr_t size) {
    handler = entry;
    event = false;
    oplen = size;
  }
};

// instruction sequence
using Instructions = std::vector<Instruction>;

// the executable representation of an AetherBinary instance
struct BasicBlocks {
  // the guest basic block vm address
  std::vector<addr_t> vmaddrs;
  // the dynamic handlers represent original instructions in those blocks
  std::vector<Instructions> handlers;
};

class Orchestrator {
public:
  struct Cache {
    // the current used blocks cache
    const BasicBlocks *current;
    // for fast lookup
    struct {
      addr_t vmaddr;
      const Instruction *insn;
    } L1[256];
  };

  // the cache
  static thread_local Cache cache;

  // all the encoded basic blocks
  std::vector<BasicBlocks> basicblocks;

public:
  static Orchestrator *inst() {
    static Orchestrator single;
    return &single;
  }

  // encode the bin into a sequence of blocks
  void encode(const Binary *bin, addr_t addend, EventConfig eventcfg);

  // find the target Instruction of the given vmaddr
  const Instruction *find(addr_t vmaddr) const;

private:
  // returned if find with an invalid vmaddr
  Instruction terminate;

  Orchestrator();
  ~Orchestrator() {}

  static const Instruction *findCache(addr_t vmaddr);
  static const Instruction *findBlocks(const BasicBlocks &blocks,
                                       addr_t vmaddr);

  Orchestrator(const Orchestrator &) = delete;
  Orchestrator &operator=(const Orchestrator &) = delete;
};

struct CPUState;

} // namespace aether

#define AETHER_ASM __asm__ __volatile__
#define AETHER_NAKED __attribute__((naked))

// the host event handler use the same ABI defined in remill/BC/ABI.h
#define DECL_EVENT_TWIN(n)                                                     \
  AETHER_NAKED void n(void);                                                   \
  extern "C" const aether::Instruction *host_##n(                              \
      void *state, addr_t vmaddr, const aether::Instruction *current);

#define IMPL_EVENT_HOST(n)                                                     \
  const aether::Instruction *host_##n(void *state, addr_t vmaddr,              \
                                      const aether::Instruction *current)

#define AETHER_VM_ENTRY()                                                      \
  AETHER_NAKED void aether_vm_entry(                                           \
      void *state, addr_t vmaddr, const Instruction *insns,                    \
      uintptr_t *host_retaddr, void *(*vm_retaddr)())

#define extract_handler_x16                                                    \
  "ldr x16, [x27]\n"                                                           \
  "ubfx x16, x16, #0, #59\n"

#define decl_cpu() auto cpu = (aether::CPUState *)((int64_t)state - 0x10)
#define forward_event(n) host_##n(state, vmaddr, current)
#define forward_event_default() &current[1]

// event events
DECL_EVENT_TWIN(event_func_before);
DECL_EVENT_TWIN(event_func_after);
DECL_EVENT_TWIN(event_insn_before);
DECL_EVENT_TWIN(event_insn_after);
DECL_EVENT_TWIN(event_syscall_before);
DECL_EVENT_TWIN(event_syscall_after);
DECL_EVENT_TWIN(event_trap_before);
DECL_EVENT_TWIN(event_trap_after);
DECL_EVENT_TWIN(event_block_before);
DECL_EVENT_TWIN(event_block_after);
DECL_EVENT_TWIN(syscall_interpret);
DECL_EVENT_TWIN(jump_interpret);
DECL_EVENT_TWIN(call_interpret);
DECL_EVENT_TWIN(finish_function);
DECL_EVENT_TWIN(finish_emulation);
DECL_EVENT_TWIN(terminate_execution);

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Orchestrator.h"
#include "Lifter.h"
#include <Utils.h>

namespace aether {

using enum EventType;

thread_local Orchestrator::Cache Orchestrator::cache{.current = nullptr};

static void do_setup_event(Instructions *handlers, EventConfig eventcfg,
                           EventType type, event_func_t event) {
  switch (type) {
  case InsnBefore:
  case InsnAfter:
    if (eventcfg.insn)
      handlers->push_back(Instruction{event});
    break;
  case BlockBefore:
  case BlockAfter:
    if (eventcfg.block)
      handlers->push_back(Instruction{event});
    break;
  case FuncBefore:
  case FuncAfter:
    if (eventcfg.func)
      handlers->push_back(Instruction{event});
    break;
  case SyscallBefore:
  case SyscallAfter:
    if (eventcfg.syscall)
      handlers->push_back(Instruction{event});
    break;
  case TrapBefore:
  case TrapAfter:
    if (eventcfg.trap)
      handlers->push_back(Instruction{event});
    break;
  default:
    break;
  }
}

#define setup_event(event, name)                                               \
  do_setup_event(handlers, eventcfg, event, event_##name)

Orchestrator::Orchestrator() : terminate{terminate_execution} {}

void Orchestrator::encode(const Binary *bin, addr_t addend,
                          EventConfig eventcfg) {
  basicblocks.push_back(BasicBlocks{});

  auto dynhandlers = bin->archType() == ARM64 ? &Lifter::aarch64 : &Lifter::x86;
  auto &current = *basicblocks.rbegin();
  for (auto &[addr, func] : bin->functions()) {
    auto fnbuf = bin->addrBuff(addr);
    // function entry block
    current.vmaddrs.push_back(addr + addend);
    current.handlers.push_back(Instructions{});
    auto handlers = &*current.handlers.rbegin();
    // before function
    setup_event(FuncBefore, func_before);
    // before basic block
    setup_event(BlockBefore, block_before);
    for (auto i = func.insns.data(), e = i + func.insns.size(); i != e; i++) {
      if (i != func.insns.data() && i->comins.size()) {
        // this instruction is referenced by other basic block, indicating the
        // end of current basic block
        // after basic block
        setup_event(BlockAfter, block_after);

        // start a new basic block
        current.vmaddrs.push_back(addr + i->fnoff + addend);
        current.handlers.push_back(Instructions{});
        handlers = &*current.handlers.rbegin();

        // before basic block
        setup_event(BlockBefore, block_before);
      }

      // before instruction
      switch (i->info.type) {
      case aether::SYSCALL:
        setup_event(SyscallBefore, syscall_before);
        break;
      case aether::TRAP:
        setup_event(TrapBefore, trap_before);
        break;
      default:
        setup_event(InsnBefore, insn_before);
        break;
      }

      // the debugger handler
      if (eventcfg.debug)
        handlers->push_back(Instruction{event_debugging});

      // the real handler
      HandlerDynamic tmp;
      std::memcpy(&tmp.opc4, fnbuf + i->fnoff, i->info.oplen);
      auto found = dynhandlers->find(tmp);
      auto entry = found == dynhandlers->end()
                       ? reinterpret_cast<uintptr_t>(terminate_execution)
                       : found->entry;
      handlers->push_back(Instruction{entry, i->info.oplen});

      // after instruction
      switch (i->info.type) {
      case aether::SYSCALL:
        if (bin->archType() == ARM64) {
          // arm64 syscall is interpreted by syscall_interpret whereas x86_64
          // by __remill_sync_hyper_call
          handlers->push_back(Instruction{syscall_interpret});
        }
        setup_event(SyscallAfter, syscall_after);
        break;
      case aether::TRAP:
        if (bin->archType() == X86_64) {
          // x86_64 trap is interpreted by interrupt_interpret whereas arm64 by
          // __remill_sync_hyper_call
          handlers->push_back(Instruction{interrupt_interpret});
        }
        setup_event(TrapAfter, trap_after);
        break;
      default:
        setup_event(InsnAfter, insn_after);
        break;
      }
      // end of basic block or function call
      switch (i->info.type) {
      case aether::JCOND:
      case aether::JUMP:
        // the end of basic block execution
        handlers->push_back(Instruction{jump_interpret});
        break;
      case aether::CALL:
        // local or host call
        handlers->push_back(Instruction{call_interpret});
        break;
      case aether::RET:
        // after function
        setup_event(FuncAfter, func_after);
        // the end of function execution
        handlers->push_back(Instruction{finish_function});
        break;
      default:
        break;
      }
    }
  }
  if (bin->functions().size() == 1) {
    // put an extra end of emulation event in case user's snippet
    // has no explicit RET instruction
    auto handlers = &*current.handlers.rbegin();
    handlers->push_back(Instruction{finish_emulation});
  }
  auto &[addr, lastfunc] = *bin->functions().rbegin();
  current.maxaddr = lastfunc.end + addend;
  cache.current = &current;
}

static inline addr_t cache_index(addr_t vmaddr) { return vmaddr & 0xFF; }

const Instruction *Orchestrator::findCache(addr_t vmaddr) {
  // L1 cache
  auto index = cache_index(vmaddr);
  auto &ref = cache.L1[index];
  if (ref.vmaddr == vmaddr && ref.insn)
    return ref.insn;

  // the current whole blocks
  return findBlocks(*cache.current, vmaddr);
}

const Instruction *Orchestrator::findBlocks(const BasicBlocks &blocks,
                                            addr_t vmaddr) {
  // address range precheck
  if (vmaddr < blocks.vmaddrs[0] || vmaddr >= blocks.maxaddr)
    return nullptr;

  auto found =
      binary_search(blocks.vmaddrs.data(), blocks.vmaddrs.size(), vmaddr);
  if (is_exact(found, blocks.vmaddrs.data(), blocks.vmaddrs.size(), vmaddr)) {
    auto insn = blocks.handlers[found - blocks.vmaddrs.data()].data();
    // cache the target insn
    cache.L1[cache_index(vmaddr)] = {vmaddr, insn};
    return insn;
  }

  // it's in the middle of found[-1] or first block
  if (found != blocks.vmaddrs.data())
    found--;

  auto index = found - blocks.vmaddrs.data();
  auto addr = *found;
  for (auto &insn : blocks.handlers[index]) {
    addr += insn.oplen;
    if (addr == vmaddr) {
      auto ptr = &insn - 1;
      // check whether the previous handler is an event or not
      if (ptr >= blocks.handlers[index].data() && !ptr->event)
        ptr++; // restore to the original handler
      // cache the target insn
      cache.L1[cache_index(vmaddr)] = {vmaddr, ptr};
      return ptr;
    }
  }
  return nullptr;
}

const Instruction *Orchestrator::find(addr_t vmaddr) const {
  // find in the current cache
  if (cache.current) {
    if (auto found = findCache(vmaddr))
      return found;
  }

  // find in all blocks
  for (auto &c : basicblocks) {
    if (cache.current == &c)
      continue;

    if (auto found = findBlocks(c, vmaddr)) {
      // cache the selected blocks
      cache.current = &c;
      return found;
    }
  }

  // Oops...
  return &terminate;
}

} // namespace aether

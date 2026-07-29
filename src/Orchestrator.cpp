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

static void add_hooker(Instructions &handlers, EventConfig eventcfg,
                       EventType type, hooker_func_t hooker) {
  switch (type) {
  case InsnBefore:
  case InsnAfter:
    if (eventcfg.insn)
      handlers.push_back(Instruction{hooker});
    break;
  case BlockBefore:
  case BlockAfter:
    if (eventcfg.block)
      handlers.push_back(Instruction{hooker});
    break;
  default:
    break;
  }
}

Orchestrator::Orchestrator() : terminate{terminate_execution} {}

void Orchestrator::encode(const Binary *bin, addr_t addend,
                          EventConfig eventcfg) {
  basicblocks.push_back(BasicBlocks{});

  auto dynhandlers = bin->archType() == ARM64 ? &Lifter::aarch64 : &Lifter::x86;
  auto copy_opcode =
      bin->archType() == ARM64
          ? [](const Insinfo *i, const char *opcptr,
               HandlerDynamic &tmp) { tmp.opc4 = *(uint32_t *)opcptr; }
          : [](const Insinfo *i, const char *opcptr, HandlerDynamic &tmp) {
              switch (i->info.oplen) {
              case 1:
                tmp.opc4 = *(uint8_t *)opcptr;
                break;
              case 2:
                tmp.opc4 = *(uint16_t *)opcptr;
                break;
              case 4:
                tmp.opc4 = *(uint32_t *)opcptr;
                break;
              case 8:
                tmp.opc8 = *(uint64_t *)opcptr;
                break;
              default:
                std::memcpy(&tmp.opc4, opcptr, i->info.oplen);
                break;
              }
            };
  auto opcode_size = bin->archType() == ARM64
                         ? [](const Insinfo *i) { return (uint16_t)0; }
                         : [](const Insinfo *i) { return i->info.oplen; };
  auto &current = *basicblocks.rbegin();
  for (auto &[addr, func] : bin->functions()) {
    auto fnbuf = bin->addrBuff(addr);
    // function entry block
    current.vmaddrs.push_back(addr + addend);
    current.handlers.push_back(Instructions{});
    auto &handlers = *current.handlers.rbegin();
    // before basic block
    add_hooker(handlers, eventcfg, BlockBefore, exec_block_before);
    for (auto i = func.insns.data(), e = i + func.insns.size(); i != e; i++) {
      // before instruction
      add_hooker(handlers, eventcfg, InsnBefore, exec_insn_before);

      HandlerDynamic tmp;
      copy_opcode(i, fnbuf + i->fnoff, tmp);
      auto found = dynhandlers->find(tmp);
      auto entry = found == dynhandlers->end()
                       ? reinterpret_cast<uintptr_t>(terminate_execution)
                       : found->entry;
      if (i != func.insns.data() && i->comins.size()) {
        // this instruction is referenced by other basic block, indicating the
        // end of current basic block
        // after instruction
        add_hooker(handlers, eventcfg, InsnAfter, exec_insn_after);
        // after basic block
        add_hooker(handlers, eventcfg, BlockAfter, exec_block_after);

        // start a new basic block
        current.vmaddrs.push_back(addr + i->fnoff + addend);
        current.handlers.push_back(Instructions{});
        handlers = *current.handlers.rbegin();

        // before basic block
        add_hooker(handlers, eventcfg, BlockBefore, exec_block_before);
        // before instruction
        add_hooker(handlers, eventcfg, InsnBefore, exec_insn_before);
      }
      // the real handler
      handlers.push_back(Instruction{entry, opcode_size(i)});
      // after instruction
      add_hooker(handlers, eventcfg, InsnAfter, exec_insn_after);
    }
  }

  cache.current = &current;
}

static inline addr_t cache_index(addr_t vmaddr) { return vmaddr & 0xFF; }

const Instruction *Orchestrator::findCache(addr_t vmaddr) {
  // L1 cache
  auto index = cache_index(vmaddr);
  auto &ref = cache.L1[index];
  if (ref.vmaddr == vmaddr)
    return ref.insn;

  // the current whole blocks
  return findBlocks(*cache.current, vmaddr);
}

const Instruction *Orchestrator::findBlocks(const BasicBlocks &blocks,
                                            addr_t vmaddr) {
  auto lastbb = blocks.handlers.rbegin();
  auto start = blocks.vmaddrs[0];
  auto end = *blocks.vmaddrs.rbegin();
  if (lastbb->begin()->oplen) {
    // x86_64
    for (auto insn : *lastbb)
      end += insn.oplen;
  } else {
    // arm64
    end += 4 * lastbb->size();
  }
  if (vmaddr < start || vmaddr >= end)
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
      // check whether the previous handler is a hooker or not
      if (!ptr->hooker)
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

  // find in the whole blockss
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

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Orchestrator.h"
#include <Utils.h>

namespace aether {

thread_local Orchestrator::Cache Orchestrator::cache{.current = nullptr};

Orchestrator::Orchestrator() {
  terminate.handler = reinterpret_cast<uintptr_t>(terminate_execution);
}

void Orchestrator::encode(const Binary *bin, addr_t addend) {
  cache.current = chains.data();
}

static inline addr_t cache_index(addr_t vmaddr) { return vmaddr & 0xFF; }

const Instruction *Orchestrator::findCache(const BasicBlock &tmpbb) {
  // L1 cache
  auto index = cache_index(tmpbb.vmaddr);
  auto &ref = cache.L1[index];
  if (ref.vmaddr == tmpbb.vmaddr)
    return ref.insn;

  // the current whole chain
  auto current = cache.current;
  auto found = binary_search(current->data(), current->size(), tmpbb);
  if (is_exact(found, current->data(), current->size(), tmpbb))
    return found->handlers.data();

  return nullptr;
}

const Instruction *Orchestrator::find(addr_t vmaddr) const {
  auto tmpbb = reinterpret_cast<BasicBlock *>(&vmaddr);
  // find in the current cache
  if (cache.current) {
    if (auto found = findCache(*tmpbb))
      return found;
  }

  // find in the whole chains
  for (auto &c : chains) {
    auto firstbb = c.begin();
    auto lastbb = c.rbegin();
    auto start = firstbb->vmaddr;
    auto end = lastbb->vmaddr;
    if (firstbb->handlers.begin()->oplen) {
      // x86_64
      for (auto insn : lastbb->handlers)
        end += insn.oplen;
    } else {
      // arm64
      end += 4 * lastbb->handlers.size();
    }
    if (vmaddr < start || vmaddr >= end)
      continue;
    // cache the selected chain
    cache.current = &c;

    auto found = binary_search(c.data(), c.size(), *tmpbb);
    if (is_exact(found, c.data(), c.size(), *tmpbb))
      return found->handlers.data();

    // it's in the middle of found[-1]
    found--;
    auto addr = found->vmaddr;
    if (found->handlers.begin()->oplen) {
      // x86_64
      for (auto &insn : found->handlers) {
        addr += insn.oplen;
        if (addr == vmaddr) {
          // cache the target insn
          cache.L1[cache_index(vmaddr)] = {vmaddr, &insn};
          return &insn;
        }
      }
      break;
    } else {
      // arm64
      auto insn = &found->handlers[(vmaddr - addr) / 4];
      // cache the target insn
      cache.L1[cache_index(vmaddr)] = {vmaddr, insn};
      return insn;
    }
  }

  // Oops...
  return &terminate;
}

} // namespace aether

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
  chains.push_back(BlockChain{});

  auto &current = *chains.rbegin();
  for (auto &func : bin->functions()) {
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

  // the current whole chain
  return findChain(*cache.current, vmaddr);
}

const Instruction *Orchestrator::findChain(const BlockChain &chain,
                                           addr_t vmaddr) {
  auto lastbb = chain.handlers.rbegin();
  auto start = chain.vmaddrs[0];
  auto end = *chain.vmaddrs.rbegin();
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
  // cache the selected chain
  cache.current = &chain;

  auto found =
      binary_search(chain.vmaddrs.data(), chain.vmaddrs.size(), vmaddr);
  if (is_exact(found, chain.vmaddrs.data(), chain.vmaddrs.size(), vmaddr)) {
    auto insn = chain.handlers[found - chain.vmaddrs.data()].data();
    // cache the target insn
    cache.L1[cache_index(vmaddr)] = {vmaddr, insn};
    return insn;
  }

  // it's in the middle of found[-1]
  found--;
  auto index = found - chain.vmaddrs.data();
  auto addr = *found;
  if (chain.handlers[index].begin()->oplen) {
    // x86_64
    for (auto &insn : chain.handlers[index]) {
      addr += insn.oplen;
      if (addr == vmaddr) {
        // cache the target insn
        cache.L1[cache_index(vmaddr)] = {vmaddr, &insn};
        return &insn;
      }
    }
  } else {
    // arm64
    auto insn = &chain.handlers[index][(vmaddr - addr) / 4];
    // cache the target insn
    cache.L1[cache_index(vmaddr)] = {vmaddr, insn};
    return insn;
  }
  return nullptr;
}

const Instruction *Orchestrator::find(addr_t vmaddr) const {
  // find in the current cache
  if (cache.current) {
    if (auto found = findCache(vmaddr))
      return found;
  }

  // find in the whole chains
  for (auto &c : chains) {
    if (auto found = findChain(c, vmaddr))
      return found;
  }

  // Oops...
  return &terminate;
}

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Orchestrator.h"
#include <Utils.h>

namespace aether {

Orchestrator::Orchestrator() {
  terminate.handler = reinterpret_cast<uintptr_t>(terminate_execution);
}

void Orchestrator::encode(const Binary *bin, addr_t addend) {
  current = chains.data();
}

const Instruction *Orchestrator::find(addr_t vmaddr) {
  auto tmpbb = reinterpret_cast<BasicBlock *>(&vmaddr);
  // find in the current cache
  auto found = binary_search(current->data(), current->size(), *tmpbb);
  if (is_exact(found, current->data(), current->size(), *tmpbb))
    return found->handlers.data();

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
    current = &c;

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
        if (addr == vmaddr)
          return &insn;
      }
    } else {
      // arm64
      return &found->handlers[(vmaddr - addr) / 4];
    }
  }

  // Oops...
  return &terminate;
}

} // namespace aether

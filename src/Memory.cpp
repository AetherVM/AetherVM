// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Memory.h"

namespace aether {

GuestMemory::GuestMemory() {
  basePointer = page_alloc(GUEST_SIZE);
  m_offset = 0;
}

GuestMemory::~GuestMemory() {
  if (!basePointer)
    return;

  page_dealloc(basePointer, GUEST_SIZE);
  basePointer = 0;
  m_offset = 0;
}

bool GuestMemory::commit(uintptr_t vmaddr, size_t size, bool read, bool write) {
  // OOM, no enough space for this size commit
  if (m_offset + size >= GUEST_SIZE)
    return false;

  auto offset = vmaddr - basePointer;
  // OOM, no enough space for this vmaddr+size commit
  if (offset + size >= GUEST_SIZE)
    return false;

  void *hostptr = reinterpret_cast<void *>(vmaddr);
  m_offset += size;

  // won't let guest have executable page anyway
  return page_commit(hostptr, size, read, write, false);
}

} // namespace aether

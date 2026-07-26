// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "Platform.h"

namespace aether {

class GuestMemory {
public:
  // 4GB reservation for guest code and segment/section memory
  static constexpr size_t GUEST_SIZE = 4ul * 1024 * 1024 * 1024;

  // The base vm address of guest
  uintptr_t baseGuest = 0x10000;

  // The base host address for the guest
  uintptr_t basePointer = 0;

  GuestMemory();
  ~GuestMemory();

  // Disable copy
  GuestMemory(const GuestMemory &) = delete;
  GuestMemory &operator=(const GuestMemory &) = delete;

  // Check whether [vmaddr, vmaddr + size) is in the right range
  bool valid(uintptr_t vmaddr, size_t size) {
    return vmaddr - baseGuest + size < GUEST_SIZE;
  }

  // The current available address for guest
  uintptr_t guestAvailable() { return baseGuest + m_offset; }

  // Convert guest to host address
  uintptr_t host(uintptr_t vmaddr) { return basePointer + vmaddr - baseGuest; }

  // Commit physical RAM to a specific region inside the reserved space
  bool commit(uintptr_t vmaddr, size_t size, bool read, bool write);

private:
  // the available offset for next commit
  size_t m_offset;
};

} // namespace aether

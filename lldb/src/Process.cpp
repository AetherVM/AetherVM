// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Process.h"

namespace lldb_private {

void AetherProcess::AttachThread(uintptr_t *pcptr, bool arm64) {
  m_threads.insert(std::make_pair(
      std::this_thread::get_id(),
      std::make_unique<AetherThread>(*this, m_tid++, pcptr, arm64)));
}

void AetherProcess::DetachThread() {
  auto found = m_threads.find(std::this_thread::get_id());
  m_threads.erase(found);
}

} // namespace lldb_private

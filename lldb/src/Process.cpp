// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Process.h"

namespace lldb_private {

#define lock_on() std::lock_guard<std::mutex> _lock_(m_mutex)

static thread_local AetherThread *thisThread = nullptr;

void AetherProcess::AttachThread(uintptr_t *pcptr, bool arm64) {
  lock_on();

  m_threads.insert(std::make_pair(
      std::this_thread::get_id(),
      std::make_unique<AetherThread>(*this, m_tid++, pcptr, arm64)));
}

void AetherProcess::DetachThread() {
  lock_on();

  auto found = m_threads.find(std::this_thread::get_id());
  m_threads.erase(found);
  thisThread = nullptr;
}

void AetherProcess::WatchDog(uintptr_t pc) {
  if (!thisThread) {
    lock_on();

    thisThread = m_threads.find(std::this_thread::get_id())->second.get();
  }

  thisThread->WatchDog(pc);
}

} // namespace lldb_private

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Process.h"

namespace lldb_private {

#define lock_on() std::lock_guard<std::mutex> _lock_(m_mutex)

static thread_local AetherThread *thisThread = nullptr;

AetherProcess::AetherProcess(lldb::pid_t pid,
                             NativeProcessProtocol::NativeDelegate &delegate)
    : NativeProcessProtocol(pid, -1, delegate) {
  SetState(lldb::eStateStopped, false);
}

void AetherProcess::AttachThread(uintptr_t *pcptr, bool arm64) {
  lock_on();

  m_threads.push_back(
      std::make_unique<AetherThread>(*this, m_tid++, pcptr, arm64));
}

void AetherProcess::DetachThread() {
  lock_on();

  auto found =
      std::find_if(m_threads.begin(), m_threads.end(),
                   [](const std::unique_ptr<NativeThreadProtocol> &thread) {
                     auto aether =
                         reinterpret_cast<AetherThread *>(thread.get());
                     return aether->GetNativeID() == std::this_thread::get_id();
                   });
  m_threads.erase(found);
  thisThread = nullptr;
}

void AetherProcess::WatchDog(uintptr_t pc) {
  if (!thisThread) {
    lock_on();

    thisThread = ThisThread();
  }

  thisThread->WatchDog(pc);
}

AetherThread *AetherProcess::ThisThread() {
  return reinterpret_cast<AetherThread *>(
      std::find_if(m_threads.begin(), m_threads.end(),
                   [](const std::unique_ptr<NativeThreadProtocol> &thread) {
                     auto aether =
                         reinterpret_cast<AetherThread *>(thread.get());
                     return aether->GetNativeID() == std::this_thread::get_id();
                   })
          ->get());
}

} // namespace lldb_private

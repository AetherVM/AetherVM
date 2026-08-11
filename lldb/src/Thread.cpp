// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Thread.h"

namespace lldb_private {

AetherThread::AetherThread(NativeProcessProtocol &process, lldb::tid_t tid,
                           uintptr_t *pcptr, bool arm64)
    : NativeThreadProtocol(process, tid), m_pcptr(pcptr),
      m_reg_context(arm64 ? reinterpret_cast<NativeRegisterContext *>(
                                new AetherRegisterContextARM64(*this))
                          : reinterpret_cast<NativeRegisterContext *>(
                                new AetherRegisterContextX64(*this))) {
  m_id = std::this_thread::get_id();
  m_stop_info.reason = lldb::eStopReasonNone;
  m_stop_info.signo = 0;
  m_stop_description = "Finished attaching";
}

bool AetherThread::GetStopReason(ThreadStopInfo &stop_info,
                                 std::string &description) {
  description.clear();

  switch (m_state) {
  case lldb::eStateStopped:
  case lldb::eStateCrashed:
  case lldb::eStateExited:
  case lldb::eStateSuspended:
  case lldb::eStateUnloaded:
    stop_info = m_stop_info;
    description = m_stop_description;
    return true;
  case lldb::eStateInvalid:
  case lldb::eStateConnected:
  case lldb::eStateAttaching:
  case lldb::eStateLaunching:
  case lldb::eStateRunning:
  case lldb::eStateStepping:
  case lldb::eStateDetached:
    return false;
  }
  llvm_unreachable("unhandled StateType!");
}

void AetherThread::WatchDog(uintptr_t pc) {
  switch (m_state) {
  case lldb::eStateStopped: {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condvar.wait(lock);
    break;
  }
  default:
    abort();
  }
}

} // namespace lldb_private

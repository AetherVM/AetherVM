// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Thread.h"
#include "Process.h"

namespace lldb_private {

AetherThread::AetherThread(NativeProcessProtocol &process, lldb::tid_t tid,
                           void *cpu, bool arm64)
    : NativeThreadProtocol(process, tid), m_cpu(cpu), m_arm64(arm64),
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
    static_cast<AetherProcess &>(GetProcess()).ReportStopped(*this);
    m_condvar.wait(lock);
    break;
  }
  case lldb::eStateStepping: {
    m_state = lldb::eStateStopped;
    m_stop_description = "Single Stepped";
    WatchDog(pc);
    break;
  }
  case lldb::eStateRunning:
    break;
  default:
    abort();
  }
}

Status AetherThread::Resume(uint32_t signo) {
  m_state = lldb::eStateRunning;
  m_condvar.notify_all();
  return Status();
}

Status AetherThread::SingleStep(uint32_t signo) {
  m_state = lldb::eStateStepping;
  m_condvar.notify_all();
  return Status();
}

} // namespace lldb_private

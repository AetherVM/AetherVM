// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "lldb/Host/common/NativeRegisterContext.h"
#include "lldb/Host/common/NativeThreadProtocol.h"
#include "lldb/Utility/Status.h"

#include "Context.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace lldb_private {

class AetherThread : public NativeThreadProtocol {
public:
  AetherThread(NativeProcessProtocol &process, lldb::tid_t tid, void *cpu,
               bool arm64);

  NativeRegisterContext &GetRegisterContext() override {
    return *m_reg_context;
  }

  std::string GetName() override {
    return std::format("AetherThread-{}", GetID());
  }

  lldb::StateType GetState() override { return m_state; }

  bool GetStopReason(ThreadStopInfo &stop_info,
                     std::string &description) override;

  Status SetWatchpoint(lldb::addr_t addr, size_t size, uint32_t watch_flags,
                       bool hardware) override {
    return Status("Watchpoint is unsupported");
  }

  Status RemoveWatchpoint(lldb::addr_t addr) override {
    return Status("Watchpoint is unsupported");
  }

  Status SetHardwareBreakpoint(lldb::addr_t addr, size_t size) override {
    abort();
    return Status();
  }

  Status RemoveHardwareBreakpoint(lldb::addr_t addr) override {
    abort();
    return Status();
  }

  void WatchDog(uintptr_t pc);

  std::thread::id GetNativeID() const { return m_id; }

  void *GetCPU() const { return m_cpu; }

  bool IsARM64() const { return m_arm64; }
  bool IsX64() const { return !IsARM64(); }

  Status Resume(uint32_t signo);
  Status SingleStep(uint32_t signo);
  void HitBreakpoint(uintptr_t pc);
  void Interrupt();

private:
  void *m_cpu = nullptr;
  bool m_arm64;
  lldb::StateType m_state = lldb::eStateStopped;
  ThreadStopInfo m_stop_info;
  std::string m_stop_description;
  std::thread::id m_id;
  std::unique_ptr<NativeRegisterContext> m_reg_context;
  std::mutex m_mutex;
  std::condition_variable m_condvar;
};

} // namespace lldb_private

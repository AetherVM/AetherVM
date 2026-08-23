// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "Thread.h"
#include "lldb/Host/common/NativeProcessProtocol.h"

#include <map>
#include <thread>

namespace lldb_private {

class AetherProcess : public NativeProcessProtocol {
public:
  AetherProcess(lldb::pid_t pid,
                NativeProcessProtocol::NativeDelegate &delegate);

  // Read VM memory space directly into LLDB buffer
  Status ReadMemory(lldb::addr_t addr, void *buf, size_t size,
                    size_t &bytes_read) override;

  // Write memory (e.g., when debugger sets software breakpoint)
  Status WriteMemory(lldb::addr_t addr, const void *buf, size_t size,
                     size_t &bytes_written) override {
    abort();
    return Status();
  }

  // Resume execution across all VCPUs
  Status Resume(const ResumeActionList &resume_actions) override;

  // Halt execution (e.g., user pressed Ctrl+C in LLDB)
  Status Interrupt() override {
    abort();
    // Pause VM execution loop
    SetState(lldb::eStateStopped);
    return Status();
  }

  // Architecture details (e.g., x86_64, arm64, riscv64)
  const ArchSpec &GetArchitecture() const override;

  Status Halt() override {
    abort();
    return Status();
  }

  Status Detach() override;

  Status Signal(int signo) override {
    abort();
    return Status();
  }

  Status Kill() override;

  size_t UpdateThreads() override { return m_threads.size(); }

  lldb::addr_t GetSharedLibraryInfoAddress() override {
    abort();
    return 0;
  }

  Status SetBreakpoint(lldb::addr_t addr, uint32_t size,
                       bool hardware) override;
  Status RemoveBreakpoint(lldb::addr_t addr, bool hardware) override;

  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>>
  GetAuxvData() const override {
    abort();
  }

  Status GetLoadedModuleFileSpec(const char *module_path,
                                 FileSpec &file_spec) override {
    abort();
    return Status();
  }

  Status GetFileLoadAddress(const llvm::StringRef &file_name,
                            lldb::addr_t &load_addr) override {
    abort();
    return Status();
  }

  Status GetMemoryRegionInfo(lldb::addr_t load_addr,
                             MemoryRegionInfo &range_info) override;

  void AttachThread(void *cpu, bool arm64);
  void DetachThread();

  void WatchDog(uintptr_t pc);

  AetherThread *ThisThread() const;
  AetherThread *MainThread() const {
    return reinterpret_cast<AetherThread *>(m_threads[0].get());
  }

  Status ResumeThread(AetherThread &thread, lldb::StateType state, int signo);
  void ReportStopped(AetherThread &thread);

private:
  lldb::tid_t m_tid = 1;
  std::mutex m_mutex;
};

} // namespace lldb_private

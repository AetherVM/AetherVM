// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "Thread.h"
#include "lldb/Host/common/NativeProcessProtocol.h"

namespace lldb_private {

class VMProcess : public NativeProcessProtocol {
public:
  VMProcess(lldb::pid_t pid, MainLoop &mainloop)
      : NativeProcessProtocol(pid, -1, m_delegate), m_mainloop(mainloop) {

    // Add default VCPU thread
    auto thread = std::make_unique<VMThread>(*this, 1);
    m_threads.push_back(std::move(thread));
  }

  // Read VM memory space directly into LLDB buffer
  Status ReadMemory(lldb::addr_t addr, void *buf, size_t size,
                    size_t &bytes_read) override {
    abort();
    return Status();
  }

  // Write memory (e.g., when debugger sets software breakpoint)
  Status WriteMemory(lldb::addr_t addr, const void *buf, size_t size,
                     size_t &bytes_written) override {
    abort();
    return Status();
  }

  // Resume execution across all VCPUs
  Status Resume(const ResumeActionList &resume_actions) override {
    abort();
    // Trigger VM execution loop until breakpoint or interrupt
    SetState(lldb::eStateRunning);
    return Status();
  }

  // Halt execution (e.g., user pressed Ctrl+C in LLDB)
  Status Interrupt() override {
    abort();
    // Pause VM execution loop
    SetState(lldb::eStateStopped);
    return Status();
  }

  // Architecture details (e.g., x86_64, arm64, riscv64)
  const ArchSpec &GetArchitecture() const override { abort(); }

  Status Halt() override {
    abort();
    return Status();
  }

  Status Detach() override {
    abort();
    return Status();
  }

  Status Signal(int signo) override {
    abort();
    return Status();
  }

  Status Kill() override {
    abort();
    return Status();
  }

  size_t UpdateThreads() override {
    // In a real implementation, this would query the VM for active threads
    return m_threads.size();
  }

  lldb::addr_t GetSharedLibraryInfoAddress() override {
    abort();
    return 0;
  }

  Status SetBreakpoint(lldb::addr_t addr, uint32_t size,
                       bool hardware) override {
    abort();
    return Status();
  }

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

private:
  struct Delegate : public NativeProcessProtocol::NativeDelegate {
    void InitializeDelegate(NativeProcessProtocol *process) override {
      abort();
    }
    void DidExec(NativeProcessProtocol *process) override { abort(); }
    void NewSubprocess(
        NativeProcessProtocol *parent_process,
        std::unique_ptr<NativeProcessProtocol> child_process) override {
      abort();
    }
    void ProcessStateChanged(NativeProcessProtocol *process,
                             lldb::StateType state) override {
      abort();
    }
  } m_delegate;

  MainLoop &m_mainloop;
  std::vector<std::unique_ptr<VMThread>> m_threads;
};

} // namespace lldb_private

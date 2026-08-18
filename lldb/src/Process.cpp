// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Process.h"
#include <AetherVM.h>
#include <Utils.h>

#include "lldb/Target/MemoryRegionInfo.h"

using namespace llvm;

namespace lldb_private {

#define lock_on()                                                              \
  std::lock_guard<std::mutex> _lock_(const_cast<AetherProcess *>(this)->m_mutex)

static thread_local AetherThread *thisThread = nullptr;

AetherProcess::AetherProcess(lldb::pid_t pid,
                             NativeProcessProtocol::NativeDelegate &delegate)
    : NativeProcessProtocol(pid, -1, delegate) {
  SetState(lldb::eStateStopped, false);
  SetCurrentThreadID(m_tid);
  AetherRegisterContext::InitOffsets();
}

void AetherProcess::AttachThread(void *cpu, bool arm64) {
  lock_on();

  m_threads.push_back(
      std::make_unique<AetherThread>(*this, m_tid++, cpu, arm64));
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
  if (found != m_threads.end())
    m_threads.erase(found);
  else
    std::cerr << std::format("[AetherDbg] Failed to find thread {} to detach.",
                             (void *)thisThread)
              << std::endl;
  thisThread = nullptr;
}

void AetherProcess::WatchDog(uintptr_t pc) { ThisThread()->WatchDog(pc); }

AetherThread *AetherProcess::ThisThread() const {
  if (thisThread)
    return thisThread;

  lock_on();
  thisThread = reinterpret_cast<AetherThread *>(
      std::find_if(m_threads.begin(), m_threads.end(),
                   [](const std::unique_ptr<NativeThreadProtocol> &thread) {
                     auto aether =
                         reinterpret_cast<AetherThread *>(thread.get());
                     return aether->GetNativeID() == std::this_thread::get_id();
                   })
          ->get());
  return thisThread;
}

const ArchSpec &AetherProcess::GetArchitecture() const {
  static std::unique_ptr<ArchSpec> arch;
  if (arch)
    return *arch;

  Triple triple;
  triple.setVendor(
#if AETHER_OS_WINDOWS
      Triple::VendorType::PC
#elif AETHER_OS_DARWIN
      Triple::VendorType::Apple
#else
      Triple::VendorType::UnknownVendor
#endif
  );
  triple.setArch(MainThread()->IsARM64() ? Triple::ArchType::aarch64
                                         : Triple::ArchType::x86_64);
  triple.setOS(
#if AETHER_OS_WINDOWS
      Triple::OSType::Win32
#elif AETHER_OS_DARWIN
      Triple::OSType::MacOSX
#else
      Triple::OSType::Linux
#endif
  );
  arch = std::make_unique<ArchSpec>(triple);
  return *arch;
}

Status AetherProcess::ReadMemory(lldb::addr_t addr, void *buf, size_t size,
                                 size_t &bytes_read) {
  auto rdbuf = Engine->readMemory(addr, size);
  if (rdbuf.size()) {
    std::memcpy(buf, rdbuf.data(), size);
    bytes_read = size;
    return Status();
  }
  bytes_read = 0;
  return Status(std::format("Invalid address 0x{:x}", addr));
}

Status AetherProcess::GetMemoryRegionInfo(lldb::addr_t load_addr,
                                          MemoryRegionInfo &range_info) {
  auto page = aether::align_down(load_addr, aether::page_size());
  range_info.GetRange().SetRangeBase(page);
  range_info.GetRange().SetByteSize(aether::page_size());
  range_info.SetReadable(MemoryRegionInfo::OptionalBool::eYes);
  range_info.SetWritable(MemoryRegionInfo::OptionalBool::eYes);
  range_info.SetExecutable(MemoryRegionInfo::OptionalBool::eYes);
  range_info.SetMapped(MemoryRegionInfo::OptionalBool::eYes);
  range_info.SetPageSize(aether::page_size());
  return Status();
}

Status AetherProcess::ResumeThread(AetherThread &thread, lldb::StateType state,
                                   int signo) {
  switch (state) {
  case lldb::eStateRunning: {
    Status resume_result = thread.Resume(signo);
    if (resume_result.Success())
      SetState(lldb::eStateRunning, true);
    return resume_result;
  }
  case lldb::eStateStepping: {
    Status step_result = thread.SingleStep(signo);
    if (step_result.Success())
      SetState(lldb::eStateRunning, true);
    return step_result;
  }
  default:
    break;
  }
  abort();
}

Status AetherProcess::Resume(const ResumeActionList &resume_actions) {
  for (const auto &thread : m_threads) {
    const ResumeAction *const action =
        resume_actions.GetActionForThread(thread->GetID(), true);
    if (action == nullptr)
      continue;

    switch (action->state) {
    case lldb::eStateRunning:
    case lldb::eStateStepping: {
      return ResumeThread(static_cast<AetherThread &>(*thread), action->state,
                          action->signal);
      break;
    }
    default:
      break;
    }
  }
  return Status();
}

void AetherProcess::ReportStopped(AetherThread &thread) {
  SetCurrentThreadID(thread.GetID());
  SetState(lldb::eStateStopped, true);
}

} // namespace lldb_private

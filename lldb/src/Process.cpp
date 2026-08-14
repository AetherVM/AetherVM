// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Process.h"
#include <AetherVM.h>

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
  m_threads.erase(found);
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
    return Status();
  }
  return Status(std::format("Invalid address 0x{:x}", addr));
}

} // namespace lldb_private

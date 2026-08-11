// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Manager.h"

namespace lldb_private {

llvm::Expected<std::unique_ptr<NativeProcessProtocol>>
AetherProcessManager::Attach(
    lldb::pid_t pid, NativeProcessProtocol::NativeDelegate &native_delegate) {
  auto proc = std::make_unique<AetherProcess>(pid, native_delegate);
  m_proc = proc.get();
  return std::move(proc);
}

} // namespace lldb_private

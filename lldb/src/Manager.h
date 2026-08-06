// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "lldb/Host/MainLoop.h"
#include "lldb/Host/ProcessLaunchInfo.h"
#include "lldb/Host/common/NativeProcessProtocol.h"
#include "lldb/Utility/Status.h"
#include "llvm/Support/Error.h"

#include <memory>

namespace lldb_private {

class AetherProcessManager : public NativeProcessProtocol::Manager {
public:
  AetherProcessManager(MainLoop &mainloop)
      : NativeProcessProtocol::Manager(mainloop) {}

  ~AetherProcessManager() override = default;

  NativeProcessProtocol::Extension GetSupportedExtensions() const override {
    abort();
    return NativeProcessProtocol::Extension::multiprocess;
  }

  llvm::Expected<std::unique_ptr<NativeProcessProtocol>>
  Launch(ProcessLaunchInfo &launch_info,
         NativeProcessProtocol::NativeDelegate &native_delegate) override {
    abort();
  }

  llvm::Expected<std::unique_ptr<NativeProcessProtocol>>
  Attach(lldb::pid_t pid,
         NativeProcessProtocol::NativeDelegate &native_delegate) override {
    abort();
  }
};

} // namespace lldb_private

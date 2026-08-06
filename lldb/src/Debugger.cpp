// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Debugger.h"
#include "Manager.h"
#include "Plugins/Process/gdb-remote/GDBRemoteCommunicationServerLLGS.h"
#include "Process.h"
#include "lldb/Host/ConnectionFileDescriptor.h"
#include "lldb/Host/MainLoop.h"

#include "Undefine.cpp"

#include <AetherVM.h>
#include <thread>

using namespace lldb_private;
using namespace lldb_private::process_gdb_remote;

namespace {

struct DebuggingContext {
  AetherDbgContext *context = nullptr;
} dbgContext;

void insn_handler(void) {}

void debugging_proc(void) {
  MainLoop mainloop;
  AetherProcessManager manager(mainloop);

  auto vm_process = std::make_unique<AetherProcess>(20260805, mainloop);

  GDBRemoteCommunicationServerLLGS server(mainloop, manager);
  mainloop.Run();
}

} // namespace

void aether_dbgmain(AetherDbgContext *context) {
  context->insn_handler = insn_handler;

  dbgContext.context = context;

  std::thread([]() { debugging_proc(); }).detach();
}

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
#include <iostream>
#include <thread>

using namespace lldb_private;
using namespace process_gdb_remote;

namespace {

struct DebuggingContext {
  AetherDbgContext *context = nullptr;
  AetherProcess *proc = nullptr;
} dbgContext;

void thread_handler(uintptr_t *pcptr) {
  if (pcptr) {
    // thread starting
    dbgContext.proc->AttachThread(pcptr, dbgContext.context->arm64);
  } else {
    // thread stopped
    dbgContext.proc->DetachThread();
  }
}

void insn_handler(void *state, uintptr_t pc, const void *insn) {}

void debugging_proc(void) {
  MainLoop mainloop;
  AetherProcessManager manager(mainloop);
  GDBRemoteCommunicationServerLLGS server(mainloop, manager);

  // AetherDbg and AetherVM are running in the same process, a fake PID fits
  server.AttachToProcess(20260805);
  dbgContext.proc = manager.CurrentProcess();

  auto connection = std::make_unique<ConnectionFileDescriptor>();
  auto url = std::format("listen://0.0.0.0:{}", dbgContext.context->port);
  Status status;
  lldb::ConnectionStatus conn_status = connection->Connect(url, &status);

  if (conn_status == lldb::eConnectionStatusSuccess && status.Success()) {
    server.SetConnection(std::move(connection));
    mainloop.Run();
  } else {
    std::cerr << "Fatal error occurred when initializing aether debugger "
                 "server socket."
              << std::endl;
  }
}

} // namespace

void aether_dbgmain(AetherDbgContext *context) {
  context->thread_handler = thread_handler;
  context->insn_handler = insn_handler;

  dbgContext.context = context;

  std::thread([]() { debugging_proc(); }).detach();
}

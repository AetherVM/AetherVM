// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Debugger.h"
#include "Manager.h"
#include "Plugins/Process/gdb-remote/GDBRemoteCommunicationServerLLGS.h"
#include "Process.h"
#include "lldb/Host/ConnectionFileDescriptor.h"
#include "lldb/Host/HostInfoBase.h"
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

  MainLoop mainloop;
  AetherProcessManager manager;
  GDBRemoteCommunicationServerLLGS server;

  DebuggingContext() : manager(mainloop), server(mainloop, manager) {}

  void initialize(uintptr_t *pcptr);
} dbgContext;

void thread_handler(uintptr_t *pcptr);

void DebuggingContext::initialize(uintptr_t *pcptr) {
  auto connection = std::make_unique<ConnectionFileDescriptor>();
  auto url = std::format("listen://0.0.0.0:{}", context->port);
  Status status;
  std::cout << "Aether Debugger - " << url << std::endl;
  // wait for lldb/Cutter client to connect
  lldb::ConnectionStatus conn_status = connection->Connect(url, &status);
  if (conn_status == lldb::eConnectionStatusSuccess && status.Success()) {
    HostInfoBase::Initialize(nullptr);
    // attach AetherProcess itself
    server.AttachToProcess(aether::current_pid());
    // initialize the first thread
    proc = manager.CurrentProcess();
    thread_handler(pcptr);
    server.InitializeConnection(std::move(connection));

    // dispatch debug event process in a new thread
    std::thread([]() { dbgContext.mainloop.Run(); }).detach();
  } else {
    std::cerr << "Fatal error occurred when initializing aether debugger "
                 "server socket."
              << std::endl;
    std::exit(-1);
  }
}

void thread_handler(uintptr_t *pcptr) {
  if (!dbgContext.proc) {
    // debugging initialization
    dbgContext.initialize(pcptr);
    return;
  }
  if (pcptr) {
    // thread starting
    dbgContext.proc->AttachThread(pcptr, dbgContext.context->arm64);
  } else {
    // thread stopped
    dbgContext.proc->DetachThread();
  }
}

void insn_handler(void *state, uintptr_t pc, const void *insn) {
  dbgContext.proc->WatchDog(pc);
}

} // namespace

void aether_dbgmain(AetherDbgContext *context) {
  context->thread_handler = thread_handler;
  context->insn_handler = insn_handler;

  dbgContext.context = context;
}

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Plugins/Process/gdb-remote/GDBRemoteCommunicationServerLLGS.h"
#include "Plugins/Process/gdb-remote/ProcessGDBRemoteLog.h"
#include "lldb/Host/ConnectionFileDescriptor.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfoBase.h"
#include "lldb/Host/MainLoop.h"
#include "lldb/Host/StreamFile.h"
#include "lldb/Utility/Log.h"

#include "Debugger.h"
#include "Manager.h"
#include "Process.h"
#include "Undefine.cpp"

#include <AetherVM.h>
#include <iostream>
#include <thread>

using namespace lldb_private;
using namespace process_gdb_remote;

namespace lldb_private {

class AetherDbgServer : public GDBRemoteCommunicationServerLLGS {
public:
  AetherDbgServer(MainLoop &loop, AetherProcessManager &manager);

protected:
  std::vector<std::string> HandleFeatures(
      const llvm::ArrayRef<llvm::StringRef> client_features) override;

  GDBRemoteCommunication::PacketResult
  Handle_qProcessInfo_VM(StringExtractorGDBRemote &packet);
};

AetherDbgServer::AetherDbgServer(MainLoop &loop, AetherProcessManager &manager)
    : GDBRemoteCommunicationServerLLGS(loop, manager) {
  RegisterMemberFunctionHandler(
      StringExtractorGDBRemote::eServerPacketType_qProcessInfo,
      &AetherDbgServer::Handle_qProcessInfo_VM);

#if 0
  auto log_handler_sp =
      std::make_shared<StreamLogHandler>(fileno(stdout), false);
  process_gdb_remote::ProcessGDBRemoteLog::Initialize();
  Log::EnableLogChannel(log_handler_sp, 0, "gdb-remote", {"packets", nullptr},
                        llvm::errs());
#endif
}

std::vector<std::string> AetherDbgServer::HandleFeatures(
    const llvm::ArrayRef<llvm::StringRef> client_features) {
  std::vector<std::string> ret =
      GDBRemoteCommunicationServerLLGS::HandleFeatures(client_features);
  ret.push_back("hwbreak+");
  return ret;
}

GDBRemoteCommunication::PacketResult
AetherDbgServer::Handle_qProcessInfo_VM(StringExtractorGDBRemote &packet) {
  // Fail if we don't have a current process.
  if (!m_current_process ||
      (m_current_process->GetID() == LLDB_INVALID_PROCESS_ID))
    return SendErrorResponse(68);

  lldb::pid_t pid = m_current_process->GetID();

  if (pid == LLDB_INVALID_PROCESS_ID)
    return SendErrorResponse(1);

  ProcessInstanceInfo proc_info;
  if (!Host::GetProcessInfo(pid, proc_info))
    return SendErrorResponse(1);

  // reset to guest's architecture
  proc_info.SetArchitecture(m_current_process->GetArchitecture());

  StreamString response;
  CreateProcessInfoResponse_DebugServerStyle(proc_info, response);
  return SendPacketNoLock(response.GetString());
}

struct DebuggingContext {
  AetherDbgContext *context = nullptr;
  AetherProcess *proc = nullptr;

  MainLoop mainloop;
  AetherProcessManager manager;
  AetherDbgServer server;
  bool detached = false;

  DebuggingContext() : manager(mainloop), server(mainloop, manager) {}

  void initialize(void *cpu);
} dbgContext;

void thread_handler(void *cpu);

void DebuggingContext::initialize(void *cpu) {
  auto connection = std::make_unique<ConnectionFileDescriptor>();
  auto url = std::format("listen://0.0.0.0:{}", context->port);
  Status status;
  std::cout << "Aether Debugger - " << url << std::endl;
  // wait for lldb/Cutter client to connect
  lldb::ConnectionStatus conn_status = connection->Connect(url, &status);
  if (conn_status == lldb::eConnectionStatusSuccess && status.Success()) {
    HostInfoBase::Initialize(nullptr);
    FileSystem::Initialize();
    // attach AetherProcess itself
    server.AttachToProcess(aether::current_pid());
    // initialize the first thread
    proc = manager.CurrentProcess();
    thread_handler(cpu);
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

void proc_detach() { dbgContext.detached = true; }

void thread_handler(void *cpu) {
  if (dbgContext.detached)
    return;

  if (!dbgContext.proc) {
    // debugging initialization
    dbgContext.initialize(cpu);
    return;
  }
  if (cpu) {
    // thread starting
    dbgContext.proc->AttachThread(cpu, dbgContext.context->arm64);
  } else {
    // thread stopped
    dbgContext.proc->DetachThread();
  }
}

void insn_handler(void *state, uintptr_t pc, const void *insn) {
  if (dbgContext.detached)
    return;

  dbgContext.proc->WatchDog(pc);
}

} // namespace lldb_private

void aether_dbgmain(AetherDbgContext *context) {
  context->thread_handler = thread_handler;
  context->insn_handler = insn_handler;

  dbgContext.context = context;
  Engine = context->engine;
}

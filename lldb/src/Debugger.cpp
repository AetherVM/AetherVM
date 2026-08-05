// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Manager.h"
#include "Plugins/Process/gdb-remote/GDBRemoteCommunicationServerLLGS.h"
#include "Process.h"
#include "lldb/Host/ConnectionFileDescriptor.h"
#include "lldb/Host/MainLoop.h"

#include "Undefine.cpp"

using namespace lldb_private;
using namespace lldb_private::process_gdb_remote;

void aether_dbgmain(void) {
  MainLoop mainloop;
  ProcessManager manager(mainloop);

  auto vm_process = std::make_unique<VMProcess>(20260805, mainloop);

  GDBRemoteCommunicationServerLLGS server(mainloop, manager);
  mainloop.Run();
}

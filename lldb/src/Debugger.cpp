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
#include "lldb/Target/RegisterFlags.h"
#include "lldb/Utility/LLDBLog.h"
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

  // rewrite LLGS's target XML to be compatible with the standard GDB Remote
  // Protocol
  llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>> BuildTargetXml_VM();
  llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
  ReadXferObject_VM(llvm::StringRef object, llvm::StringRef annex);

  GDBRemoteCommunication::PacketResult
  Handle_qProcessInfo_VM(StringExtractorGDBRemote &packet);

  GDBRemoteCommunication::PacketResult
  Handle_qXfer_VM(StringExtractorGDBRemote &packet);
};

AetherDbgServer::AetherDbgServer(MainLoop &loop, AetherProcessManager &manager)
    : GDBRemoteCommunicationServerLLGS(loop, manager) {
  RegisterMemberFunctionHandler(
      StringExtractorGDBRemote::eServerPacketType_qProcessInfo,
      &AetherDbgServer::Handle_qProcessInfo_VM);
  RegisterMemberFunctionHandler(
      StringExtractorGDBRemote::eServerPacketType_qXfer,
      &AetherDbgServer::Handle_qXfer_VM);

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

static llvm::StringRef GetEncodingNameOrEmpty(const RegisterInfo &reg_info) {
  using namespace lldb;
  switch (reg_info.encoding) {
  case eEncodingUint:
    return "uint";
  case eEncodingSint:
    return "sint";
  case eEncodingIEEE754:
    return "ieee754";
  case eEncodingVector:
    return "vector";
  default:
    return "";
  }
}

static llvm::StringRef GetFormatNameOrEmpty(const RegisterInfo &reg_info) {
  using namespace lldb;
  switch (reg_info.format) {
  case eFormatDefault:
    return "";
  case eFormatBoolean:
    return "boolean";
  case eFormatBinary:
    return "binary";
  case eFormatBytes:
    return "bytes";
  case eFormatBytesWithASCII:
    return "bytes-with-ascii";
  case eFormatChar:
    return "char";
  case eFormatCharPrintable:
    return "char-printable";
  case eFormatComplex:
    return "complex";
  case eFormatCString:
    return "cstring";
  case eFormatDecimal:
    return "decimal";
  case eFormatEnum:
    return "enum";
  case eFormatHex:
    return "hex";
  case eFormatHexUppercase:
    return "hex-uppercase";
  case eFormatFloat:
    return "float";
  case eFormatOctal:
    return "octal";
  case eFormatOSType:
    return "ostype";
  case eFormatUnicode16:
    return "unicode16";
  case eFormatUnicode32:
    return "unicode32";
  case eFormatUnsigned:
    return "unsigned";
  case eFormatPointer:
    return "pointer";
  case eFormatVectorOfChar:
    return "vector-char";
  case eFormatVectorOfSInt64:
    return "vector-sint64";
  case eFormatVectorOfFloat16:
    return "vector-float16";
  case eFormatVectorOfFloat64:
    return "vector-float64";
  case eFormatVectorOfSInt8:
    return "vector-sint8";
  case eFormatVectorOfUInt8:
    return "vector-uint8";
  case eFormatVectorOfSInt16:
    return "vector-sint16";
  case eFormatVectorOfUInt16:
    return "vector-uint16";
  case eFormatVectorOfSInt32:
    return "vector-sint32";
  case eFormatVectorOfUInt32:
    return "vector-uint32";
  case eFormatVectorOfFloat32:
    return "vector-float32";
  case eFormatVectorOfUInt64:
    return "vector-uint64";
  case eFormatVectorOfUInt128:
    return "vector-uint128";
  case eFormatComplexInteger:
    return "complex-integer";
  case eFormatCharArray:
    return "char-array";
  case eFormatAddressInfo:
    return "address-info";
  case eFormatHexFloat:
    return "hex-float";
  case eFormatInstruction:
    return "instruction";
  case eFormatVoid:
    return "void";
  case eFormatUnicode8:
    return "unicode8";
  case eFormatFloat128:
    return "float128";
  default:
    llvm_unreachable("Unknown register format");
  };
}

static llvm::StringRef GetKindGenericOrEmpty(const RegisterInfo &reg_info) {
  using namespace lldb;
  switch (reg_info.kinds[RegisterKind::eRegisterKindGeneric]) {
  case LLDB_REGNUM_GENERIC_PC:
    return "pc";
  case LLDB_REGNUM_GENERIC_SP:
    return "sp";
  case LLDB_REGNUM_GENERIC_FP:
    return "fp";
  case LLDB_REGNUM_GENERIC_RA:
    return "ra";
  case LLDB_REGNUM_GENERIC_FLAGS:
    return "flags";
  case LLDB_REGNUM_GENERIC_ARG1:
    return "arg1";
  case LLDB_REGNUM_GENERIC_ARG2:
    return "arg2";
  case LLDB_REGNUM_GENERIC_ARG3:
    return "arg3";
  case LLDB_REGNUM_GENERIC_ARG4:
    return "arg4";
  case LLDB_REGNUM_GENERIC_ARG5:
    return "arg5";
  case LLDB_REGNUM_GENERIC_ARG6:
    return "arg6";
  case LLDB_REGNUM_GENERIC_ARG7:
    return "arg7";
  case LLDB_REGNUM_GENERIC_ARG8:
    return "arg8";
  case LLDB_REGNUM_GENERIC_TP:
    return "tp";
  default:
    return "";
  }
}

static void CollectRegNums(const uint32_t *reg_num, StreamString &response,
                           bool usehex) {
  for (int i = 0; *reg_num != LLDB_INVALID_REGNUM; ++reg_num, ++i) {
    if (i > 0)
      response.PutChar(',');
    if (usehex)
      response.Printf("%" PRIx32, *reg_num);
    else
      response.Printf("%" PRIu32, *reg_num);
  }
}

llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
AetherDbgServer::BuildTargetXml_VM() {
  using namespace llvm;
  using namespace lldb;
  // Ensure we have a thread.
  NativeThreadProtocol *thread = m_current_process->GetThreadAtIndex(0);
  if (!thread)
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "No thread available");

  Log *log = GetLog(LLDBLog::Process | LLDBLog::Thread);
  // Get the register context for the first thread.
  NativeRegisterContext &reg_context = thread->GetRegisterContext();

  // XML Feature namespace
  const char *reg_type = nullptr, *reg_group = nullptr;
  auto gpr_namespace = "org.gnu.gdb.aarch64.core";
  auto fpu_namespace = "org.gnu.gdb.aarch64.fpu";
  auto fpu_reg0 = "v0";
  auto fpu_types = R"(<vector id="v2d" type="ieee_double" count="2"/>
<vector id="v4f" type="ieee_single" count="4"/>
<vector id="v16i8" type="int8" count="16"/>
<union id="aarch64v">
  <field name="f" type="v4f"/>
  <field name="d" type="v2d"/>
  <field name="u" type="v16i8"/>
</union>)";
  auto fpu_type = "aarch64v";
  auto fpu_group = "fpu";
  if (m_current_process->GetArchitecture().GetMachine() ==
      llvm::Triple::x86_64) {
    gpr_namespace = "org.gnu.gdb.i386.core";
    fpu_namespace = "org.gnu.gdb.i386.sse";
    fpu_reg0 = "xmm0";
    fpu_types = R"(<vector id="v4f" type="ieee_single" count="4"/>
<vector id="v2d" type="ieee_double" count="2"/>
<vector id="v16i8" type="int8" count="16"/>
<vector id="v8i16" type="int16" count="8"/>
<vector id="v4i32" type="int32" count="4"/>
<vector id="v2i64" type="int64" count="2"/>
<union id="vec128">
  <field name="v4_float" type="v4f"/>
  <field name="v2_double" type="v2d"/>
  <field name="v16_int8" type="v16i8"/>
  <field name="v8_int16" type="v8i16"/>
  <field name="v4_int32" type="v4i32"/>
  <field name="v2_int64" type="v2i64"/>
  <field name="uint128" type="uint128"/>
</union>)";
    fpu_type = "vec128";
    fpu_group = "sse";
  }

  StreamString response;

  response.Printf("<?xml version=\"1.0\"?>\n");
  response.Printf("<target version=\"1.0\">\n");
  response.IndentMore();

  response.Indent();
  response.Printf("<architecture>%s</architecture>\n",
                  m_current_process->GetArchitecture()
                      .GetTriple()
                      .getArchName()
                      .str()
                      .c_str());

  response.Printf("<feature name=\"%s\">\n", gpr_namespace);

  const int registers_count = reg_context.GetUserRegisterCount();
  if (registers_count)
    response.IndentMore();

  llvm::StringSet<> field_enums_seen;
  for (int reg_index = 0; reg_index < registers_count; reg_index++) {
    const RegisterInfo *reg_info =
        reg_context.GetRegisterInfoAtIndex(reg_index);

    if (!reg_info) {
      LLDB_LOGF(log,
                "%s failed to get register info for register index %" PRIu32,
                "target.xml", reg_index);
      continue;
    }

    // Switch from GPR to FPU
    if (strcmp(reg_info->name, fpu_reg0) == 0) {
      reg_type = fpu_type;
      reg_group = fpu_group;
      response.Printf("</feature>\n<feature name=\"%s\">\n%s\n", fpu_namespace,
                      fpu_types);
    }

    if (reg_info->flags_type) {
      response.IndentMore();
      reg_info->flags_type->EnumsToXML(response, field_enums_seen);
      reg_info->flags_type->ToXML(response);
      response.IndentLess();
    }

    response.Indent();
    response.Printf("<reg name=\"%s\" bitsize=\"%" PRIu32 "\" regnum=\"%d\" ",
                    reg_info->name, reg_info->byte_size * 8, reg_index);

    if (!reg_context.RegisterOffsetIsDynamic())
      response.Printf("offset=\"%" PRIu32 "\" ", reg_info->byte_offset);

    if (reg_info->alt_name && reg_info->alt_name[0])
      response.Printf("altname=\"%s\" ", reg_info->alt_name);

    llvm::StringRef encoding = GetEncodingNameOrEmpty(*reg_info);
    if (!encoding.empty())
      response << "encoding=\"" << encoding << "\" ";

    llvm::StringRef format = GetFormatNameOrEmpty(*reg_info);
    if (!format.empty())
      response << "format=\"" << format << "\" ";

    if (reg_type && reg_info->byte_size >= 16)
      response << "type=\"" << reg_type << "\" ";
    else if (reg_info->flags_type)
      response << "type=\"" << reg_info->flags_type->GetID() << "\" ";

    if (reg_group) {
      response << "group=\"" << reg_group << "\" ";
    } else {
      const char *const register_set_name =
          reg_context.GetRegisterSetNameForRegisterAtIndex(reg_index);
      if (register_set_name)
        response << "group=\"" << register_set_name << "\" ";
    }

    if (reg_info->kinds[RegisterKind::eRegisterKindEHFrame] !=
        LLDB_INVALID_REGNUM)
      response.Printf("ehframe_regnum=\"%" PRIu32 "\" ",
                      reg_info->kinds[RegisterKind::eRegisterKindEHFrame]);

    if (reg_info->kinds[RegisterKind::eRegisterKindDWARF] !=
        LLDB_INVALID_REGNUM)
      response.Printf("dwarf_regnum=\"%" PRIu32 "\" ",
                      reg_info->kinds[RegisterKind::eRegisterKindDWARF]);

    llvm::StringRef kind_generic = GetKindGenericOrEmpty(*reg_info);
    if (!kind_generic.empty())
      response << "generic=\"" << kind_generic << "\" ";

    if (reg_info->value_regs &&
        reg_info->value_regs[0] != LLDB_INVALID_REGNUM) {
      response.PutCString("value_regnums=\"");
      CollectRegNums(reg_info->value_regs, response, false);
      response.Printf("\" ");
    }

    if (reg_info->invalidate_regs && reg_info->invalidate_regs[0]) {
      response.PutCString("invalidate_regnums=\"");
      CollectRegNums(reg_info->invalidate_regs, response, false);
      response.Printf("\" ");
    }

    response.Printf("/>\n");
  }

  if (registers_count)
    response.IndentLess();

  response.Indent("</feature>\n");
  response.IndentLess();
  response.Indent("</target>\n");
  return MemoryBuffer::getMemBufferCopy(response.GetString(), "target.xml");
}

llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
AetherDbgServer::ReadXferObject_VM(llvm::StringRef object,
                                   llvm::StringRef annex) {
  using namespace llvm;
  // Make sure we have a valid process.
  if (!m_current_process ||
      (m_current_process->GetID() == LLDB_INVALID_PROCESS_ID)) {
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "No process available");
  }

  if (object == "auxv") {
    // Grab the auxv data.
    auto buffer_or_error = m_current_process->GetAuxvData();
    if (!buffer_or_error)
      return llvm::errorCodeToError(buffer_or_error.getError());
    return std::move(*buffer_or_error);
  }

  if (object == "siginfo") {
    NativeThreadProtocol *thread = m_current_process->GetCurrentThread();
    if (!thread)
      return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                     "no current thread");

    auto buffer_or_error = thread->GetSiginfo();
    if (!buffer_or_error)
      return buffer_or_error.takeError();
    return std::move(*buffer_or_error);
  }

  if (object == "libraries-svr4") {
    auto library_list = m_current_process->GetLoadedSVR4Libraries();
    if (!library_list)
      return library_list.takeError();

    StreamString response;
    response.Printf("<library-list-svr4 version=\"1.0\">");
    for (auto const &library : *library_list) {
      response.Printf("<library name=\"%s\" ",
                      XMLEncodeAttributeValue(library.name.c_str()).c_str());
      response.Printf("lm=\"0x%" PRIx64 "\" ", library.link_map);
      response.Printf("l_addr=\"0x%" PRIx64 "\" ", library.base_addr);
      response.Printf("l_ld=\"0x%" PRIx64 "\" />", library.ld_addr);
    }
    response.Printf("</library-list-svr4>");
    return MemoryBuffer::getMemBufferCopy(response.GetString(), __FUNCTION__);
  }

  if (object == "features" && annex == "target.xml")
    return BuildTargetXml_VM();

  return llvm::make_error<UnimplementedError>();
}

GDBRemoteCommunication::PacketResult
AetherDbgServer::Handle_qXfer_VM(StringExtractorGDBRemote &packet) {
  using namespace llvm;
  SmallVector<StringRef, 5> fields;
  // The packet format is "qXfer:<object>:<action>:<annex>:offset,length"
  StringRef(packet.GetStringRef()).split(fields, ':', 4);
  if (fields.size() != 5)
    return SendIllFormedResponse(packet, "malformed qXfer packet");
  StringRef &xfer_object = fields[1];
  StringRef &xfer_action = fields[2];
  StringRef &xfer_annex = fields[3];
  StringExtractor offset_data(fields[4]);
  if (xfer_action != "read")
    return SendUnimplementedResponse("qXfer action not supported");
  // Parse offset.
  const uint64_t xfer_offset =
      offset_data.GetHexMaxU64(false, std::numeric_limits<uint64_t>::max());
  if (xfer_offset == std::numeric_limits<uint64_t>::max())
    return SendIllFormedResponse(packet, "qXfer packet missing offset");
  // Parse out comma.
  if (offset_data.GetChar() != ',')
    return SendIllFormedResponse(packet,
                                 "qXfer packet missing comma after offset");
  // Parse out the length.
  const uint64_t xfer_length =
      offset_data.GetHexMaxU64(false, std::numeric_limits<uint64_t>::max());
  if (xfer_length == std::numeric_limits<uint64_t>::max())
    return SendIllFormedResponse(packet, "qXfer packet missing length");

  // Get a previously constructed buffer if it exists or create it now.
  std::string buffer_key = (xfer_object + xfer_action + xfer_annex).str();
  auto buffer_it = m_xfer_buffer_map.find(buffer_key);
  if (buffer_it == m_xfer_buffer_map.end()) {
    auto buffer_up = ReadXferObject_VM(xfer_object, xfer_annex);
    if (!buffer_up)
      return SendErrorResponse(buffer_up.takeError());
    buffer_it = m_xfer_buffer_map
                    .insert(std::make_pair(buffer_key, std::move(*buffer_up)))
                    .first;
  }

  // Send back the response
  StreamGDBRemote response;
  bool done_with_buffer = false;
  llvm::StringRef buffer = buffer_it->second->getBuffer();
  if (xfer_offset >= buffer.size()) {
    // We have nothing left to send.  Mark the buffer as complete.
    response.PutChar('l');
    done_with_buffer = true;
  } else {
    // Figure out how many bytes are available starting at the given offset.
    buffer = buffer.drop_front(xfer_offset);
    // Mark the response type according to whether we're reading the remainder
    // of the data.
    if (xfer_length >= buffer.size()) {
      // There will be nothing left to read after this
      response.PutChar('l');
      done_with_buffer = true;
    } else {
      // There will still be bytes to read after this request.
      response.PutChar('m');
      buffer = buffer.take_front(xfer_length);
    }
    // Now write the data in encoded binary form.
    response.PutEscapedBytes(buffer.data(), buffer.size());
  }

  if (done_with_buffer)
    m_xfer_buffer_map.erase(buffer_it);

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

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#define UNDEFINE_LLDB(n)                                                       \
  void n(void) { abort(); }

extern "C" {
#if __APPLE__
UNDEFINE_LLDB(_ZN12lldb_private12LinuxSignalsC1Ev);
UNDEFINE_LLDB(_ZN12lldb_private13NetBSDSignalsC1Ev);
UNDEFINE_LLDB(_ZN12lldb_private14FreeBSDSignalsC1Ev);
UNDEFINE_LLDB(_ZN12lldb_private14OpenBSDSignalsC1Ev);
UNDEFINE_LLDB(_ZN5clang14FrontendActionC2Ev);
UNDEFINE_LLDB(_ZN5clang14FrontendActionD2Ev);
UNDEFINE_LLDB(_ZN5clang15getLLVMRevisionEv);
UNDEFINE_LLDB(_ZN5clang16CompilerInstance13ExecuteActionERNS_14FrontendActionE);
UNDEFINE_LLDB(
    _ZN5clang16CompilerInstance17createDiagnosticsEPNS_18DiagnosticConsumerEb);
UNDEFINE_LLDB(
    _ZN5clang16CompilerInstanceC1ENSt3__110shared_ptrINS_18CompilerInvocationEEENS2_INS_22PCHContainerOperationsEEENS2_INS_11ModuleCacheEEE);
UNDEFINE_LLDB(_ZN5clang16CompilerInstanceD1Ev);
UNDEFINE_LLDB(
    _ZN5clang16createInvocationEN4llvm8ArrayRefIPKcEENS_23CreateInvocationOptionsE);
UNDEFINE_LLDB(_ZN5clang16getClangRevisionEv);
UNDEFINE_LLDB(_ZN5clang17DiagnosticsEngineD1Ev);
UNDEFINE_LLDB(_ZN5clang22PCHContainerOperationsC1Ev);
UNDEFINE_LLDB(
    _ZN5clang6driver6Driver25getDefaultModuleCachePathERN4llvm15SmallVectorImplIcEE);
UNDEFINE_LLDB(
    _ZNK12lldb_private23ScriptedInterfaceUsages4DumpERNS_6StreamENS0_9UsageKindE);
UNDEFINE_LLDB(_ZTVN5clang20DumpModuleInfoActionE);
UNDEFINE_LLDB(_ZTVN5clang28ObjectFilePCHContainerReaderE);
UNDEFINE_LLDB(_ZN4llvm23EnableABIBreakingChecksE);
UNDEFINE_LLDB(_ZNK4llvm5Error19fatalUncheckedErrorEv);
UNDEFINE_LLDB(_ZTVN5clang17ASTFrontendActionE);
#else
#endif
}

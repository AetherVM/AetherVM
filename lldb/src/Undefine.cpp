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
// clang-format off
#pragma comment(linker, "/alternatename:?getDefaultModuleCachePath@Driver@driver@clang@@SA_NAEAV?$SmallVectorImpl@D@llvm@@@Z=abort")
#pragma comment(linker, "/alternatename:??0FreeBSDSignals@lldb_private@@QEAA@XZ=abort")
#pragma comment(linker, "/alternatename:??0NetBSDSignals@lldb_private@@QEAA@XZ=abort")
#pragma comment(linker, "/alternatename:??0LinuxSignals@lldb_private@@QEAA@XZ=abort")
#pragma comment(linker, "/alternatename:??0OpenBSDSignals@lldb_private@@QEAA@XZ=abort")
#pragma comment(linker, "/alternatename:?getClangRevision@clang@@YA?AV?$basic_string@DU?$char_traits@D@__1@std@@V?$allocator@D@23@@__1@std@@XZ=abort")
#pragma comment(linker, "/alternatename:?getLLVMRevision@clang@@YA?AV?$basic_string@DU?$char_traits@D@__1@std@@V?$allocator@D@23@@__1@std@@XZ=abort")
#pragma comment(linker, "/alternatename:?StringFn@?$EnumTraits@W4LocationAtom@dwarf@llvm@@@dwarf@llvm@@2Q6A?AVStringRef@3@I@ZEA=abort")
#pragma comment(linker, "/alternatename:?Dump@ScriptedInterfaceUsages@lldb_private@@QEBAXAEAVStream@2@W4UsageKind@12@@Z=abort")
#pragma comment(linker, "/alternatename:??0PCHContainerOperations@clang@@QEAA@XZ=abort")
#pragma comment(linker, "/alternatename:?createInvocation@clang@@YA?AV?$unique_ptr@VCompilerInvocation@clang@@U?$default_delete@VCompilerInvocation@clang@@@__1@std@@@__1@std@@V?$ArrayRef@PEBD@llvm@@UCreateInvocationOptions@1@@Z=abort")
#pragma comment(linker, "/alternatename:??0CompilerInstance@clang@@QEAA@V?$shared_ptr@VCompilerInvocation@clang@@@__1@std@@V?$shared_ptr@VPCHContainerOperations@clang@@@34@V?$shared_ptr@VModuleCache@clang@@@34@@Z=abort")
#pragma comment(linker, "/alternatename:?createDiagnostics@CompilerInstance@clang@@QEAAXPEAVDiagnosticConsumer@2@_N@Z=abort")
#pragma comment(linker, "/alternatename:??0FrontendAction@clang@@QEAA@XZ=abort")
#pragma comment(linker, "/alternatename:?ExecuteAction@CompilerInstance@clang@@QEAA_NAEAVFrontendAction@2@@Z=abort")
#pragma comment(linker, "/alternatename:??1FrontendAction@clang@@UEAA@XZ=abort")
#pragma comment(linker, "/alternatename:??1CompilerInstance@clang@@UEAA@XZ=abort")
#pragma comment(linker, "/alternatename:??1PCHContainerReader@clang@@UEAA@XZ=abort")
#pragma comment(linker, "/alternatename:?CreateASTConsumer@DumpModuleInfoAction@clang@@MEAA?AV?$unique_ptr@VASTConsumer@clang@@U?$default_delete@VASTConsumer@clang@@@__1@std@@@__1@std@@AEAVCompilerInstance@2@VStringRef@llvm@@@Z=abort")
#pragma comment(linker, "/alternatename:?BeginInvocation@DumpModuleInfoAction@clang@@MEAA_NAEAVCompilerInstance@2@@Z=abort")
#pragma comment(linker, "/alternatename:?ExecuteAction@DumpModuleInfoAction@clang@@MEAAXXZ=abort")
#pragma comment(linker, "/alternatename:?shouldEraseOutputFiles@FrontendAction@clang@@MEAA_NXZ=abort")
#pragma comment(linker, "/alternatename:?EndSourceFile@FrontendAction@clang@@UEAAXXZ=abort")
#pragma comment(linker, "/alternatename:?getFormats@ObjectFilePCHContainerReader@clang@@EEBA?AV?$ArrayRef@VStringRef@llvm@@@llvm@@XZ=abort")
#pragma comment(linker, "/alternatename:?ExtractPCH@ObjectFilePCHContainerReader@clang@@EEBA?AVStringRef@llvm@@VMemoryBufferRef@4@@Z=abort")
#pragma comment(linker, "/alternatename:?ID@ECError@llvm@@2DA=abort")
#pragma comment(linker, "/alternatename:?ID@ErrorList@llvm@@2DA=abort")
#pragma comment(linker, "/alternatename:?ID@ErrorInfoBase@llvm@@0DA=abort")
// clang-format on
#endif
}

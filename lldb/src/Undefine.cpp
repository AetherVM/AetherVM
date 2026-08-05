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
UNDEFINE_LLDB(
    _ZN12lldb_private14HostInfoMacOSX10GetSDKRootENS_12HostInfoBase10SDKOptionsE);
UNDEFINE_LLDB(_ZN12lldb_private14HostInfoMacOSX12GetOSVersionEv);
UNDEFINE_LLDB(_ZN12lldb_private14HostInfoMacOSX16GetOSBuildStringEv);
UNDEFINE_LLDB(_ZN12lldb_private14HostInfoMacOSX18GetProgramFileSpecEv);
UNDEFINE_LLDB(_ZN12lldb_private14HostInfoMacOSX21GetMacCatalystVersionEv);
UNDEFINE_LLDB(
    _ZN12lldb_private14HostInfoMacOSX22ComputeHeaderDirectoryERNS_8FileSpecE);
UNDEFINE_LLDB(
    _ZN12lldb_private14HostInfoMacOSX26ComputeSupportExeDirectoryERNS_8FileSpecE);
UNDEFINE_LLDB(
    _ZN12lldb_private14HostInfoMacOSX27ComputeUserPluginsDirectoryERNS_8FileSpecE);
UNDEFINE_LLDB(
    _ZN12lldb_private14HostInfoMacOSX29ComputeSystemPluginsDirectoryERNS_8FileSpecE);
UNDEFINE_LLDB(
    _ZN12lldb_private14HostInfoMacOSX30ComputeHostArchitectureSupportERNS_8ArchSpecES2_);
UNDEFINE_LLDB(_ZN12lldb_private14OpenBSDSignalsC1Ev);
UNDEFINE_LLDB(_ZN12lldb_private16HostThreadMacOSX22ThreadCreateTrampolineEPv);
UNDEFINE_LLDB(_ZN12lldb_private4Host13LaunchProcessERNS_17ProcessLaunchInfoE);
UNDEFINE_LLDB(_ZN12lldb_private4Host14GetEnvironmentEv);
UNDEFINE_LLDB(
    _ZN12lldb_private4Host14GetProcessInfoEyRNS_19ProcessInstanceInfoE);
UNDEFINE_LLDB(
    _ZN12lldb_private4Host17FindProcessesImplERKNS_24ProcessInstanceInfoMatchERNSt3__16vectorINS_19ProcessInstanceInfoENS4_9allocatorIS6_EEEE);
UNDEFINE_LLDB(
    _ZN12lldb_private4Host20ShellExpandArgumentsERNS_17ProcessLaunchInfoE);
UNDEFINE_LLDB(
    _ZN12lldb_private4Host24OpenFileInExternalEditorEN4llvm9StringRefERKNS_8FileSpecEj);
UNDEFINE_LLDB(_ZN12lldb_private4Host25ResolveExecutableInBundleERNS_8FileSpecE);
UNDEFINE_LLDB(_ZN12lldb_private4Host27IsInteractiveGraphicSessionEv);
UNDEFINE_LLDB(
    _ZN12lldb_private4Host27StartMonitoringChildProcessERKNSt3__18functionIFvyiiEEEy);
UNDEFINE_LLDB(
    _ZN12lldb_private4Host9SystemLogEN4lldb8SeverityEN4llvm9StringRefE);
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
#else
#endif
}

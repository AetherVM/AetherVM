// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#if DEBUG || _DEBUG || !NDEBUG
#define AETHER_DEBUG 1
#endif

#if defined(_WIN32) || defined(_WIN64)
#define AETHER_OS_WINDOWS 1
#else
#define AETHER_OS_POSIX 1
#if defined(__APPLE__)
#define AETHER_OS_DARWIN 1
#else
#define AETHER_OS_LINUX 1
#endif
#endif

#if __arm64__ || __aarch64__
#define AETHER_ARCH_ARM64 1
#elif __x86_64__ || __x64__ || _M_AMD64 || _M_X64
#define AETHER_ARCH_X64 1
#else
#error AetherVM only supports AArch64 and X86_64
#endif

#ifdef _WIN32
#ifdef AETHER_DLLIMPL
#define AETHER_VMAPI __declspec(dllexport)
#else
#define AETHER_VMAPI __declspec(dllimport)
#endif // end of AETHER_DLLIMPL
#else
#define AETHER_VMAPI __attribute__((visibility("default")))
#endif // end of _WIN32

// Rename these symbol names if we're building AetherVM for ICPP, so that we can
// separate ICPP's runtime and the normal built AetherVM, and then AetherVM's
// consumer script of ICPP can run normally, otherwise they will have conflicts
// of the shared internal thread local CPU state.
#if ICPP_DLLIMPL
#define BinaryEngine BinaryEngineICPP
#define BinaryEngineImpl BinaryEngineImplICPP
#define CPUState CPUStateICPP
#define Lifter LifterICPP
#define GuestMemory GuestMemoryICPP
#define OpcodeEngine OpcodeEngineICPP
#define OpcodeHandlers OpcodeHandlersICPP
#define OpcodeHandler OpcodeHandlerICPP
#define Orchestrator OrchestratorICPP
#define opcode_generator opcode_generator_icpp
#define opcret_generator opcret_generator_icpp
#endif

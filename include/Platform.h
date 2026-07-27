// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#define AETHER_OS_WINDOWS 1
#else
#define AETHER_OS_POSIX 1
#if defined(__APPLE__)
#define AETHER_OS_MACOS 1
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

namespace aether {

size_t page_size();
// return 0 means failed
uintptr_t page_alloc(size_t size);
bool page_commit(void *hostptr, size_t size, bool read, bool write, bool exec);
bool page_decommit(void *hostptr, size_t size);
void page_dealloc(uintptr_t pagestart, size_t size);
std::string self_path();

} // namespace aether

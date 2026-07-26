// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#define AETHER_OS_WINDOWS 1
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#define AETHER_OS_POSIX 1
#include <sys/mman.h>
#include <unistd.h>
#if defined(__APPLE__)
#define AETHER_OS_MACOS 1
#else
#define AETHER_OS_LINUX 1
#endif
#endif

namespace aether {

size_t page_size();
// return 0 means failed
uintptr_t page_alloc(size_t size);
bool page_commit(void *hostptr, size_t size, bool read, bool write, bool exec);
bool page_decommit(void *hostptr, size_t size);
void page_dealloc(uintptr_t pagestart, size_t size);

} // namespace aether

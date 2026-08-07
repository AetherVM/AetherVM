// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "Common.h"

namespace aether {

AETHER_VMAPI size_t page_size();
// return 0 means failed
AETHER_VMAPI uintptr_t page_alloc(size_t size);
AETHER_VMAPI bool page_commit(void *hostptr, size_t size, bool read, bool write,
                              bool exec);
AETHER_VMAPI bool page_decommit(void *hostptr, size_t size);
AETHER_VMAPI void page_dealloc(uintptr_t pagestart, size_t size);

AETHER_VMAPI std::string self_path();

AETHER_VMAPI size_t stack_size();

AETHER_VMAPI const void *load_library(std::string_view path);
AETHER_VMAPI const void *resolve_symbol(const void *handle,
                                        std::string_view name);

} // namespace aether

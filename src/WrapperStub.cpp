// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "WrapperStub.h"

namespace aether {

#if AETHER_ARCH_ARM64
#define AETHER_STUB "adr x16, .\nb " HOST_CALL_PREFIX "wrapper_entry\n"
#else
#define AETHER_STUB                                                            \
  "lea (%rip), %r11\n call " HOST_CALL_PREFIX "wrapper_entry\n"
#endif

#include <generated/WrapperEntryStub.cpp>

} // namespace aether

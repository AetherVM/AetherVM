// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#define __remill_state __remill_state_aarch64

#define ISEL_UNSUPPORTED_INSTRUCTION ISEL_UNSUPPORTED_INSTRUCTION_AArch64
#define ISEL_INVALID_INSTRUCTION ISEL_INVALID_INSTRUCTION_AArch64

#include <lib/Arch/AArch64/Runtime/Instructions.cpp>

#include <generated/HandlerAArch64.cpp>

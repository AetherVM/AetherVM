// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "Orchestrator.h"

#include <Register.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace aether {

namespace aarch64 {

#include <remill/Arch/AArch64/Runtime/State.h>

AETHER_VM_ENTRY();

size_t offset_reg(Register reg);
size_t offset_reg(std::string_view reg);

} // namespace aarch64

} // namespace aether

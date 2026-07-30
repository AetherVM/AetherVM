// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "Orchestrator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace aether {

namespace aarch64 {

#include <remill/Arch/AArch64/Runtime/State.h>

AETHER_NAKED void host_vm_entry(void *cpu, addr_t vmaddr,
                                const Instruction *insns, uintptr_t *retaddr);

} // namespace aarch64

} // namespace aether

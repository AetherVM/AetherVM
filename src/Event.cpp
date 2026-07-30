// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "BinaryEngine.h"
#include "Orchestrator.h"

namespace aether {

void event_func_before() {}

void event_func_after() {}

void event_insn_before() {}

void event_insn_after() {}

void event_block_before() {}

void event_block_after() {}

void jump_interpret() {}

void call_interpret() {}

void finish_function() {}

// run to an invalid vm address
void terminate_execution() { abort(); };

} // namespace aether

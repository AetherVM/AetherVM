// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "BinaryEngine.h"
#include "Orchestrator.h"

IMPL_EVENT_HOST(event_func_before) { return current; }

IMPL_EVENT_HOST(event_func_after) { return current; }

IMPL_EVENT_HOST(event_insn_before) { return current; }

IMPL_EVENT_HOST(event_insn_after) { return current; }

IMPL_EVENT_HOST(event_block_before) { return current; }

IMPL_EVENT_HOST(event_block_after) { return current; }

IMPL_EVENT_HOST(jump_interpret) { return current; }

IMPL_EVENT_HOST(call_interpret) { return current; }

IMPL_EVENT_HOST(finish_function) { return current; }

// run to an invalid vm address
IMPL_EVENT_HOST(terminate_execution) { abort(); };

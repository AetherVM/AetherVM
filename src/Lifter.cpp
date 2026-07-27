// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Lifter.h>

namespace aether {

std::map<uint32_t, uintptr_t> arch64;
std::map<std::string, uintptr_t> x86;

Lifter::Lifter(remill::Arch *ptr) : arch(ptr) {}

Lifter::~Lifter() {}

void Lifter::transform(std::span<const uint8_t> opcode) {}

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Handler.h>

namespace aether {

std::vector<Handler> Handler::aarch64;
std::vector<Handler> Handler::x86;

void Handler::loadAArch64() {}

void Handler::loadX86() {}

} // namespace aether

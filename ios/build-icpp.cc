// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

/*
Build AetherVM for ICPP in one go, usage: icpp build-icpp.cc [Debug]
*/

#define main build_main

#include "build.cc"

#undef main

int main(int argc, const char *argv[]) { return build_main(argc, argv); }

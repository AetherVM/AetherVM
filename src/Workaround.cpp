// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <cstdio>

namespace google {
extern void (*gflags_exitfunc)(int);
}

namespace {

void gflags_nop_exit(int) {}

struct GlobalInit {
  // disable remill's gflags exiting our program for multiple flags...
  GlobalInit() { google::gflags_exitfunc = &gflags_nop_exit; }
} globalInit;

} // namespace

#if _WIN32
#else
// shut up gflags about multiple flags
extern "C" int vfprintf(FILE *stream, const char *format, va_list ap) {
  return 1;
}
#endif

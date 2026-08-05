// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#ifdef _WIN32
#ifdef AETHER_DLLIMPL
#define __AETHER_API__ __declspec(dllexport)
#else
#define __AETHER_API__ __declspec(dllimport)
#endif // end of AETHER_DLLIMPL
#else
#define __AETHER_API__ __attribute__((visibility("default")))
#endif // end of _WIN32

extern "C" __AETHER_API__ void aether_dbgmain(void);

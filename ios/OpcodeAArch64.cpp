// AetherMachO - Mach-O analysis and emulation engine for macOS/iOS
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <cstdint>

extern "C" {
extern const uint8_t arm64_native_start[];
extern const uint8_t arm64_native_end[];
}

__asm__(".section __TEXT,__text\n"
        ".globl _arm64_native_start\n"
        "_arm64_native_start:\n"
        // this path is relative to the current building directory
        ".incbin \"../build-opcode/arm64.opc.ret\"\n"
        ".globl _arm64_native_end\n"
        "_arm64_native_end:\n");

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <stdint.h>

namespace aether {

enum class Register : int {
  // General: PC,SP,FP
  PC,

  // AArch64
  X0,
  X1,
  X2,
  X3,
  X4,
  X5,
  X6,
  X7,
  X8,
  X9,
  X10,
  X11,
  X12,
  X13,
  X14,
  X15,
  X16,
  X17,
  X18,
  X19,
  X20,
  X21,
  X22,
  X23,
  X24,
  X25,
  X26,
  X27,
  X28,
  X29,
  X30,
  X31,

  NZCV,

  Q0,
  Q1,
  Q2,
  Q3,
  Q4,
  Q5,
  Q6,
  Q7,
  Q8,
  Q9,
  Q10,
  Q11,
  Q12,
  Q13,
  Q14,
  Q15,
  Q16,
  Q17,
  Q18,
  Q19,
  Q20,
  Q21,
  Q22,
  Q23,
  Q24,
  Q25,
  Q26,
  Q27,
  Q28,
  Q29,
  Q30,
  Q31,

  FP = X29,
  LR = X30,
  SP = X31,

  // X86_64
  RIP = PC,
  RBP = FP,
  RSP = SP,
  RAX,
  RBX,
  RCX,
  RDI,
  RDX,
  RSI,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,

  RFLAGS,

  ST0,
  ST1,
  ST2,
  ST3,
  ST4,
  ST5,
  ST6,
  ST7,

  MM0,
  MM1,
  MM2,
  MM3,
  MM4,
  MM5,
  MM6,
  MM7,

  // XMM is the base of YMM, which is the base of ZMM, so XMM/YMM/ZMM share the
  // same XMM0-XMM31 name in AetherVM.
  XMM0,
  XMM1,
  XMM2,
  XMM3,
  XMM4,
  XMM5,
  XMM6,
  XMM7,
  XMM8,
  XMM9,
  XMM10,
  XMM11,
  XMM12,
  XMM13,
  XMM14,
  XMM15,
  XMM16,
  XMM17,
  XMM18,
  XMM19,
  XMM20,
  XMM21,
  XMM22,
  XMM23,
  XMM24,
  XMM25,
  XMM26,
  XMM27,
  XMM28,
  XMM29,
  XMM30,
  XMM31,
};

// A general register value representation, the value type is for GPR, the
// pointer type is for NEON/SIMD/SSE vector register.
union RegisterValue {
  // unsigned byte
  uint8_t u1;
  uint16_t u2;
  uint32_t u4;
  uint64_t u8;

  // signed byte
  int8_t s1;
  int16_t s2;
  int32_t s4;
  int64_t s8;

  // byte pointer
  uint8_t *u1p;
  uint16_t *u2p;
  uint32_t *u4p;
  uint64_t *u8p;

  // signed byte pointer
  int8_t *s1p;
  int16_t *s2p;
  int32_t *s4p;
  int64_t *s8p;

  // float number
  float f;
  double d;

  // float number pointer
  float *fp;
  double *dp;

  // general pointer
  const void *ptr;

  // string pointer
  const char *str;

  // only for aarch64
  struct {
    uint64_t _0 : 28;
    uint64_t v : 1; // Result overflowed, bit 28.
    uint64_t c : 1; // Result produced a carry.
    uint64_t z : 1; // Result is zero.
    uint64_t n : 1; // Result is negative, bit 31.
    uint64_t _1 : 32;
  } NZCV;

  // only for x86_64
  struct {
    uint32_t cf : 1; // bit 0.
    uint32_t must_be_1 : 1;
    uint32_t pf : 1;
    uint32_t must_be_0a : 1;

    uint32_t af : 1; // bit 4.
    uint32_t must_be_0b : 1;
    uint32_t zf : 1;
    uint32_t sf : 1;

    uint32_t tf : 1;  // bit 8.
    uint32_t _if : 1; // underscore to avoid token clash.
    uint32_t df : 1;
    uint32_t of : 1;

    uint32_t iopl : 2; // A 2-bit field, bits 12-13.
    uint32_t nt : 1;
    uint32_t must_be_0c : 1;

    uint32_t rf : 1; // bit 16.
    uint32_t vm : 1;
    uint32_t ac : 1; // Alignment check.
    uint32_t vif : 1;

    uint32_t vip : 1;              // bit 20.
    uint32_t id : 1;               // bit 21.
    uint32_t reserved_eflags : 10; // bits 22-31.
    uint32_t reserved_rflags;      // bits 32-63.
  } rflags;
};

} // namespace aether

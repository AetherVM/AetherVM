// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "CPUState.h"
#include "Orchestrator.h"
#include <Platform.h>

#include <cfenv>
#include <cstdlib>

#if AETHER_OS_MACOS
extern "C" int syscall(int number, ...);
#else
#endif

template <typename T> static T &AccessMemory(addr_t addr) {
  return *reinterpret_cast<T *>(static_cast<uintptr_t>(addr));
}

extern "C" {

#define MAKE_RW_MEMORY(size)                                                   \
  NEVER_INLINE uint##size##_t __remill_read_memory_##size(Memory *,            \
                                                          addr_t addr) {       \
    return AccessMemory<uint##size##_t>(addr);                                 \
  }                                                                            \
  NEVER_INLINE Memory *__remill_write_memory_##size(Memory *, addr_t addr,     \
                                                    const uint##size##_t in) { \
    AccessMemory<uint##size##_t>(addr) = in;                                   \
    return nullptr;                                                            \
  }

#define MAKE_RW_FP_MEMORY(size)                                                \
  NEVER_INLINE float##size##_t __remill_read_memory_f##size(Memory *,          \
                                                            addr_t addr) {     \
    return AccessMemory<float##size##_t>(addr);                                \
  }                                                                            \
  NEVER_INLINE Memory *__remill_write_memory_f##size(Memory *, addr_t addr,    \
                                                     float##size##_t in) {     \
    AccessMemory<float##size##_t>(addr) = in;                                  \
    return nullptr;                                                            \
  }

MAKE_RW_MEMORY(8)
MAKE_RW_MEMORY(16)
MAKE_RW_MEMORY(32)
MAKE_RW_MEMORY(64)

MAKE_RW_FP_MEMORY(32)
MAKE_RW_FP_MEMORY(64)
MAKE_RW_FP_MEMORY(128)

NEVER_INLINE Memory *__remill_read_memory_f80(Memory *, addr_t addr,
                                              float80_t &out) {
  out = AccessMemory<float80_t>(addr);
  return nullptr;
}

NEVER_INLINE Memory *__remill_write_memory_f80(Memory *, addr_t addr,
                                               const float80_t &in) {
  AccessMemory<float80_t>(addr) = in;
  return nullptr;
}

Memory *__remill_compare_exchange_memory_8(Memory *memory, addr_t addr,
                                           uint8_t &expected, uint8_t desired) {
  expected = __sync_val_compare_and_swap(reinterpret_cast<uint8_t *>(addr),
                                         expected, desired);
  return memory;
}

Memory *__remill_compare_exchange_memory_16(Memory *memory, addr_t addr,
                                            uint16_t &expected,
                                            uint16_t desired) {
  expected = __sync_val_compare_and_swap(reinterpret_cast<uint16_t *>(addr),
                                         expected, desired);
  return memory;
}

Memory *__remill_compare_exchange_memory_32(Memory *memory, addr_t addr,
                                            uint32_t &expected,
                                            uint32_t desired) {
  expected = __sync_val_compare_and_swap(reinterpret_cast<uint32_t *>(addr),
                                         expected, desired);
  return memory;
}

Memory *__remill_compare_exchange_memory_64(Memory *memory, addr_t addr,
                                            uint64_t &expected,
                                            uint64_t desired) {
  expected = __sync_val_compare_and_swap(reinterpret_cast<uint64_t *>(addr),
                                         expected, desired);
  return memory;
}

Memory *__remill_compare_exchange_memory_128(Memory *memory, addr_t addr,
                                             uint128_t &expected,
                                             uint128_t &desired) {
#ifdef _GXX_EXPERIMENTAL_CXX0X__
  expected = __sync_val_compare_and_swap(reinterpret_cast<uint128_t *>(addr),
                                         expected, desired);
#endif
  return memory;
}

#define MAKE_ATOMIC_INTRINSIC(intrinsic_name, type_prefix, size)               \
  Memory *__remill_##intrinsic_name##_##size(Memory *memory, addr_t addr,      \
                                             type_prefix##size##_t &value) {   \
    value = __sync_##intrinsic_name(                                           \
        reinterpret_cast<type_prefix##size##_t *>(addr), value);               \
    return memory;                                                             \
  }

MAKE_ATOMIC_INTRINSIC(fetch_and_add, uint, 8)
MAKE_ATOMIC_INTRINSIC(fetch_and_add, uint, 16)
MAKE_ATOMIC_INTRINSIC(fetch_and_add, uint, 32)
MAKE_ATOMIC_INTRINSIC(fetch_and_add, uint, 64)
MAKE_ATOMIC_INTRINSIC(fetch_and_sub, uint, 8)
MAKE_ATOMIC_INTRINSIC(fetch_and_sub, uint, 16)
MAKE_ATOMIC_INTRINSIC(fetch_and_sub, uint, 32)
MAKE_ATOMIC_INTRINSIC(fetch_and_sub, uint, 64)
MAKE_ATOMIC_INTRINSIC(fetch_and_or, uint, 8)
MAKE_ATOMIC_INTRINSIC(fetch_and_or, uint, 16)
MAKE_ATOMIC_INTRINSIC(fetch_and_or, uint, 32)
MAKE_ATOMIC_INTRINSIC(fetch_and_or, uint, 64)
MAKE_ATOMIC_INTRINSIC(fetch_and_and, uint, 8)
MAKE_ATOMIC_INTRINSIC(fetch_and_and, uint, 16)
MAKE_ATOMIC_INTRINSIC(fetch_and_and, uint, 32)
MAKE_ATOMIC_INTRINSIC(fetch_and_and, uint, 64)
MAKE_ATOMIC_INTRINSIC(fetch_and_xor, uint, 8)
MAKE_ATOMIC_INTRINSIC(fetch_and_xor, uint, 16)
MAKE_ATOMIC_INTRINSIC(fetch_and_xor, uint, 32)
MAKE_ATOMIC_INTRINSIC(fetch_and_xor, uint, 64)

static int MapFpuExceptToFe(int32_t guest_except) {
  using enum aether::aarch64::FPUExceptionFlag;

  int host_except = 0;
  if (guest_except & kFPUExceptionInvalid)
    host_except |= FE_INVALID;
  if (guest_except & kFPUExceptionDivByZero)
    host_except |= FE_DIVBYZERO;
  if (guest_except & kFPUExceptionOverflow)
    host_except |= FE_OVERFLOW;
  if (guest_except & kFPUExceptionUnderflow)
    host_except |= FE_UNDERFLOW;
  if (guest_except & kFPUExceptionPrecision)
    host_except |= FE_INEXACT;
  // NOTE: denormal exception is not available on all architectures
#ifdef FE_DENORMALOPERAND
  if (guest_except & kFPUExceptionDenormal)
    host_except |= FE_DENORMALOPERAND;
#endif // FE_DENORMALOPERAND
#ifdef FE_DENORMAL
  if (guest_except & kFPUExceptionDenormal)
    host_except |= FE_DENORMAL;
#endif
  return host_except;
}

static int MapFeToFpuExcept(int host_except) {
  using enum aether::aarch64::FPUExceptionFlag;

  int guest_except = 0;
  if (host_except & FE_INVALID)
    guest_except |= kFPUExceptionInvalid;
  if (host_except & FE_DIVBYZERO)
    guest_except |= kFPUExceptionDivByZero;
  if (host_except & FE_OVERFLOW)
    guest_except |= kFPUExceptionOverflow;
  if (host_except & FE_UNDERFLOW)
    guest_except |= kFPUExceptionUnderflow;
  if (host_except & FE_INEXACT)
    guest_except |= kFPUExceptionPrecision;
  // NOTE: denormal exception is not available on all architectures
#ifdef FE_DENORMALOPERAND
  if (host_except & FE_DENORMALOPERAND)
    guest_except |= kFPUExceptionDenormal;
#endif // FE_DENORMALOPERAND
#ifdef FE_DENORMAL
  if (host_except & FE_DENORMAL)
    guest_except |= kFPUExceptionDenormal;
#endif
  return guest_except;
}

static int MapFpuRoundToFe(int32_t guest_round) {
  using enum aether::aarch64::FPURoundingMode;

  switch (guest_round) {
  case kFPURoundToNearestEven:
    return FE_TONEAREST;
  case kFPURoundUpInf:
    return FE_UPWARD;
  case kFPURoundDownNegInf:
    return FE_DOWNWARD;
  case kFPURoundToZero:
    return FE_TOWARDZERO;
  default:
    return FE_TONEAREST;
  }
}

static int MapFeToFpuRound(int host_round) {
  using enum aether::aarch64::FPURoundingMode;

  switch (host_round) {
  case FE_TONEAREST:
    return kFPURoundToNearestEven;
  case FE_UPWARD:
    return kFPURoundUpInf;
  case FE_DOWNWARD:
    return kFPURoundDownNegInf;
  case FE_TOWARDZERO:
    return kFPURoundToZero;
  default:
    return kFPURoundToNearestEven;
  }
}

int32_t __remill_fpu_exception_test(int32_t read_mask) {
  int host_mask = MapFpuExceptToFe(read_mask);
  int host_result = std::fetestexcept(host_mask);
  return MapFeToFpuExcept(host_result);
}

void __remill_fpu_exception_clear(int32_t clear_mask) {
  int host_mask = MapFpuExceptToFe(clear_mask);
  std::feclearexcept(host_mask);
}

void __remill_fpu_exception_raise(int32_t except_mask) {
  int host_mask = MapFpuExceptToFe(except_mask);
  std::feraiseexcept(host_mask);
}

void __remill_fpu_set_rounding(int32_t round_mode) {
  int host_mode = MapFpuRoundToFe(round_mode);
  std::fesetround(host_mode);
}

int32_t __remill_fpu_get_rounding() {
  int host_mode = std::fegetround();
  return MapFeToFpuRound(host_mode);
}

Memory *__remill_barrier_load_load(Memory *) { return nullptr; }
Memory *__remill_barrier_load_store(Memory *) { return nullptr; }
Memory *__remill_barrier_store_load(Memory *) { return nullptr; }
Memory *__remill_barrier_store_store(Memory *) { return nullptr; }
Memory *__remill_atomic_begin(Memory *) { return nullptr; }
Memory *__remill_atomic_end(Memory *) { return nullptr; }

Memory *__remill_delay_slot_begin(Memory *) { return nullptr; }
Memory *__remill_delay_slot_end(Memory *) { return nullptr; }

void __remill_defer_inlining(void) {}

Memory *__remill_error(State &, addr_t, Memory *) { abort(); }

Memory *__remill_missing_block(State &, addr_t, Memory *memory) {
  return memory;
}

// Read/write to I/O ports.
uint8_t __remill_read_io_port_8(Memory *, addr_t) { abort(); }

uint16_t __remill_read_io_port_16(Memory *, addr_t) { abort(); }

uint32_t __remill_read_io_port_32(Memory *, addr_t) { abort(); }

Memory *__remill_write_io_port_8(Memory *, addr_t, uint8_t) { abort(); }

Memory *__remill_write_io_port_16(Memory *, addr_t, uint16_t) { abort(); }

Memory *__remill_write_io_port_32(Memory *, addr_t, uint32_t) { abort(); }

Memory *__remill_function_call(State &, addr_t, Memory *) { abort(); }

AETHER_NAKED Memory *__remill_function_return(State &, addr_t, Memory *) {
  // for the register context, see AArch64.cpp and X86.cpp
#if AETHER_ARCH_ARM64
  // advance the current executable insn and call it
  AETHER_ASM("add x2, x2, #8\n"
             "mov x27, x2\n"
             "" extract_handler_x16 ""
             "br x16");
#else
  AETHER_ASM("int3");
#endif
}

Memory *__remill_jump(State &, addr_t, Memory *) { abort(); }

Memory *__remill_async_hyper_call(State &, addr_t, Memory *) { abort(); }

uint8_t __remill_undefined_8(void) { return 0; }

uint16_t __remill_undefined_16(void) { return 0; }

uint32_t __remill_undefined_32(void) { return 0; }

uint64_t __remill_undefined_64(void) { return 0; }

float32_t __remill_undefined_f32(void) { return 0.0; }

float64_t __remill_undefined_f64(void) { return 0.0; }

float80_t __remill_undefined_f80(void) { return {0}; }

float128_t __remill_undefined_f128(void) { return 0.0; }

bool __remill_flag_computation_zero(bool result, ...) { return result; }

bool __remill_flag_computation_sign(bool result, ...) { return result; }

bool __remill_flag_computation_overflow(bool result, ...) { return result; }

bool __remill_flag_computation_carry(bool result, ...) { return result; }

bool __remill_compare_sle(bool result) { return result; }

bool __remill_compare_slt(bool result) { return result; }

bool __remill_compare_sge(bool result) { return result; }

bool __remill_compare_sgt(bool result) { return result; }

bool __remill_compare_ule(bool result) { return result; }

bool __remill_compare_ult(bool result) { return result; }

bool __remill_compare_ugt(bool result) { return result; }

bool __remill_compare_uge(bool result) { return result; }

bool __remill_compare_eq(bool result) { return result; }

bool __remill_compare_neq(bool result) { return result; }

Memory *__remill_x86_set_segment_es(Memory *) { abort(); }

Memory *__remill_x86_set_segment_ss(Memory *) { abort(); }

Memory *__remill_x86_set_segment_ds(Memory *) { abort(); }

Memory *__remill_x86_set_segment_fs(Memory *) { abort(); }

Memory *__remill_x86_set_segment_gs(Memory *) { abort(); }

Memory *__remill_x86_set_debug_reg(Memory *) { abort(); }

Memory *__remill_x86_set_control_reg_0(Memory *) { abort(); }

Memory *__remill_x86_set_control_reg_1(Memory *) { abort(); }

Memory *__remill_x86_set_control_reg_2(Memory *) { abort(); }

Memory *__remill_x86_set_control_reg_3(Memory *) { abort(); }

Memory *__remill_x86_set_control_reg_4(Memory *) { abort(); }

Memory *__remill_amd64_set_debug_reg(Memory *) { abort(); }

Memory *__remill_amd64_set_control_reg_0(Memory *) { abort(); }

Memory *__remill_amd64_set_control_reg_1(Memory *) { abort(); }

Memory *__remill_amd64_set_control_reg_2(Memory *) { abort(); }

Memory *__remill_amd64_set_control_reg_3(Memory *) { abort(); }

Memory *__remill_amd64_set_control_reg_4(Memory *) { abort(); }

Memory *__remill_amd64_set_control_reg_8(Memory *) { abort(); }

Memory *__remill_aarch64_emulate_instruction(Memory *) { abort(); }

Memory *__remill_aarch32_emulate_instruction(Memory *) { abort(); }

Memory *__remill_aarch32_check_not_el2(Memory *) { abort(); }

Memory *__remill_sparc_set_asi_register(Memory *) { abort(); }

Memory *__remill_sparc_unimplemented_instruction(Memory *) { abort(); }

Memory *__remill_sparc_unhandled_dcti(Memory *) { abort(); }

Memory *__remill_sparc_window_underflow(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_a(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_n(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_ne(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_e(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_g(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_le(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_ge(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_l(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_gu(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_leu(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_cc(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_cs(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_pos(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_neg(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_vc(Memory *) { abort(); }

Memory *__remill_sparc_trap_cond_vs(Memory *) { abort(); }

Memory *__remill_sparc32_emulate_instruction(Memory *) { abort(); }

Memory *__remill_sparc64_emulate_instruction(Memory *) { abort(); }

// Marks `mem` as being used. This is used for making sure certain symbols are
// kept around through optimization, and makes sure that optimization doesn't
// perform dead-argument elimination on any of the intrinsics.
void __remill_mark_as_used(void *mem) { asm("" ::"m"(mem)); }

Memory *__remill_sync_hyper_call(aether::x86::State &cpu, Memory *memory,
                                 SyncHyperCall::Name name) {
  // only x86_64 guest will use this remill intrinsic, arm64 guest will use
  // syscall_interpret event handler
  switch (name) {
  case SyncHyperCall::kX86SysCall:
#if AETHER_OS_MACOS
    cpu.gpr.rax.qword =
        syscall(cpu.gpr.rax.qword, cpu.gpr.rdi, cpu.gpr.rsi, cpu.gpr.rdx,
                cpu.gpr.r10, cpu.gpr.r8, cpu.gpr.r9);
#else
#error TODO:: implement __remill_sync_hyper_call for non-macOS platforms
#endif
    break;
  default:
    abort();
  }
  return memory;
}

} // extern C

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Handler.h>
#include <Utils.h>
#include <algorithm>

namespace aether {

std::vector<Handler> Handler::aarch64;
std::vector<Handler> Handler::x86;
std::vector<Handler> Handler::intrinsic;

static void load_handler(std::vector<Handler> &handlers, const HandlerRaw *raw,
                         size_t num) {
  if (handlers.size() == num)
    return;

  handlers.resize(num);
  std::transform(raw, raw + num, handlers.begin(), [](const HandlerRaw &r) {
    return Handler{hash_value(r.name), *r.handler};
  });
  std::sort(handlers.begin(), handlers.end());
}

static void load_intrinsic(std::vector<Handler> &handlers,
                           const IntrinsicRaw *raw, size_t num) {
  if (handlers.size() == num)
    return;

  handlers.resize(num);
  std::transform(raw, raw + num, handlers.begin(), [](const IntrinsicRaw &r) {
    return Handler{hash_value(r.name), r.handler};
  });
  std::sort(handlers.begin(), handlers.end());
}

void Handler::loadAArch64() {
  load_handler(aarch64, &HandlerAArch64[0], HandlerAArch64Num);
  load_intrinsic(intrinsic, &HandlerIntrinsic[0], HandlerIntrinsicNum);
}

void Handler::loadX86() {
  load_handler(x86, &HandlerX86[0], HandlerX86Num);
  load_intrinsic(intrinsic, &HandlerIntrinsic[0], HandlerIntrinsicNum);
}

} // namespace aether

#define REMILL_DECL(n) extern "C" void n(void)
#define REMILL_ITEM(n) {#n, (void *)n}

REMILL_DECL(__remill_aarch32_check_not_el2);
REMILL_DECL(__remill_aarch32_emulate_instruction);
REMILL_DECL(__remill_aarch64_emulate_instruction);
REMILL_DECL(__remill_amd64_set_control_reg_0);
REMILL_DECL(__remill_amd64_set_control_reg_1);
REMILL_DECL(__remill_amd64_set_control_reg_2);
REMILL_DECL(__remill_amd64_set_control_reg_3);
REMILL_DECL(__remill_amd64_set_control_reg_4);
REMILL_DECL(__remill_amd64_set_control_reg_8);
REMILL_DECL(__remill_amd64_set_debug_reg);
REMILL_DECL(__remill_async_hyper_call);
REMILL_DECL(__remill_atomic_begin);
REMILL_DECL(__remill_atomic_end);
REMILL_DECL(__remill_barrier_load_load);
REMILL_DECL(__remill_barrier_load_store);
REMILL_DECL(__remill_barrier_store_load);
REMILL_DECL(__remill_barrier_store_store);
REMILL_DECL(__remill_compare_eq);
REMILL_DECL(__remill_compare_exchange_memory_128);
REMILL_DECL(__remill_compare_exchange_memory_16);
REMILL_DECL(__remill_compare_exchange_memory_32);
REMILL_DECL(__remill_compare_exchange_memory_64);
REMILL_DECL(__remill_compare_exchange_memory_8);
REMILL_DECL(__remill_compare_neq);
REMILL_DECL(__remill_compare_sge);
REMILL_DECL(__remill_compare_sgt);
REMILL_DECL(__remill_compare_sle);
REMILL_DECL(__remill_compare_slt);
REMILL_DECL(__remill_compare_uge);
REMILL_DECL(__remill_compare_ugt);
REMILL_DECL(__remill_compare_ule);
REMILL_DECL(__remill_compare_ult);
REMILL_DECL(__remill_defer_inlining);
REMILL_DECL(__remill_delay_slot_begin);
REMILL_DECL(__remill_delay_slot_end);
REMILL_DECL(__remill_error);
REMILL_DECL(__remill_fetch_and_add_16);
REMILL_DECL(__remill_fetch_and_add_32);
REMILL_DECL(__remill_fetch_and_add_64);
REMILL_DECL(__remill_fetch_and_add_8);
REMILL_DECL(__remill_fetch_and_and_16);
REMILL_DECL(__remill_fetch_and_and_32);
REMILL_DECL(__remill_fetch_and_and_64);
REMILL_DECL(__remill_fetch_and_and_8);
REMILL_DECL(__remill_fetch_and_or_16);
REMILL_DECL(__remill_fetch_and_or_32);
REMILL_DECL(__remill_fetch_and_or_64);
REMILL_DECL(__remill_fetch_and_or_8);
REMILL_DECL(__remill_fetch_and_sub_16);
REMILL_DECL(__remill_fetch_and_sub_32);
REMILL_DECL(__remill_fetch_and_sub_64);
REMILL_DECL(__remill_fetch_and_sub_8);
REMILL_DECL(__remill_fetch_and_xor_16);
REMILL_DECL(__remill_fetch_and_xor_32);
REMILL_DECL(__remill_fetch_and_xor_64);
REMILL_DECL(__remill_fetch_and_xor_8);
REMILL_DECL(__remill_flag_computation_carry);
REMILL_DECL(__remill_flag_computation_overflow);
REMILL_DECL(__remill_flag_computation_sign);
REMILL_DECL(__remill_flag_computation_zero);
REMILL_DECL(__remill_fpu_exception_clear);
REMILL_DECL(__remill_fpu_exception_raise);
REMILL_DECL(__remill_fpu_exception_test);
REMILL_DECL(__remill_fpu_get_rounding);
REMILL_DECL(__remill_fpu_set_rounding);
REMILL_DECL(__remill_function_call);
REMILL_DECL(__remill_function_return);
REMILL_DECL(__remill_jump);
REMILL_DECL(__remill_mark_as_used);
REMILL_DECL(__remill_missing_block);
REMILL_DECL(__remill_read_io_port_16);
REMILL_DECL(__remill_read_io_port_32);
REMILL_DECL(__remill_read_io_port_8);
REMILL_DECL(__remill_read_memory_16);
REMILL_DECL(__remill_read_memory_32);
REMILL_DECL(__remill_read_memory_64);
REMILL_DECL(__remill_read_memory_8);
REMILL_DECL(__remill_read_memory_f128);
REMILL_DECL(__remill_read_memory_f32);
REMILL_DECL(__remill_read_memory_f64);
REMILL_DECL(__remill_read_memory_f80);
REMILL_DECL(__remill_sparc32_emulate_instruction);
REMILL_DECL(__remill_sparc64_emulate_instruction);
REMILL_DECL(__remill_sparc_set_asi_register);
REMILL_DECL(__remill_sparc_trap_cond_a);
REMILL_DECL(__remill_sparc_trap_cond_cc);
REMILL_DECL(__remill_sparc_trap_cond_cs);
REMILL_DECL(__remill_sparc_trap_cond_e);
REMILL_DECL(__remill_sparc_trap_cond_g);
REMILL_DECL(__remill_sparc_trap_cond_ge);
REMILL_DECL(__remill_sparc_trap_cond_gu);
REMILL_DECL(__remill_sparc_trap_cond_l);
REMILL_DECL(__remill_sparc_trap_cond_le);
REMILL_DECL(__remill_sparc_trap_cond_leu);
REMILL_DECL(__remill_sparc_trap_cond_n);
REMILL_DECL(__remill_sparc_trap_cond_ne);
REMILL_DECL(__remill_sparc_trap_cond_neg);
REMILL_DECL(__remill_sparc_trap_cond_pos);
REMILL_DECL(__remill_sparc_trap_cond_vc);
REMILL_DECL(__remill_sparc_trap_cond_vs);
REMILL_DECL(__remill_sparc_unhandled_dcti);
REMILL_DECL(__remill_sparc_unimplemented_instruction);
REMILL_DECL(__remill_sparc_window_underflow);
REMILL_DECL(__remill_sync_hyper_call);
REMILL_DECL(__remill_undefined_16);
REMILL_DECL(__remill_undefined_32);
REMILL_DECL(__remill_undefined_64);
REMILL_DECL(__remill_undefined_8);
REMILL_DECL(__remill_undefined_f128);
REMILL_DECL(__remill_undefined_f32);
REMILL_DECL(__remill_undefined_f64);
REMILL_DECL(__remill_undefined_f80);
REMILL_DECL(__remill_write_io_port_16);
REMILL_DECL(__remill_write_io_port_32);
REMILL_DECL(__remill_write_io_port_8);
REMILL_DECL(__remill_write_memory_16);
REMILL_DECL(__remill_write_memory_32);
REMILL_DECL(__remill_write_memory_64);
REMILL_DECL(__remill_write_memory_8);
REMILL_DECL(__remill_write_memory_f128);
REMILL_DECL(__remill_write_memory_f32);
REMILL_DECL(__remill_write_memory_f64);
REMILL_DECL(__remill_write_memory_f80);
REMILL_DECL(__remill_x86_set_control_reg_0);
REMILL_DECL(__remill_x86_set_control_reg_1);
REMILL_DECL(__remill_x86_set_control_reg_2);
REMILL_DECL(__remill_x86_set_control_reg_3);
REMILL_DECL(__remill_x86_set_control_reg_4);
REMILL_DECL(__remill_x86_set_debug_reg);
REMILL_DECL(__remill_x86_set_segment_ds);
REMILL_DECL(__remill_x86_set_segment_es);
REMILL_DECL(__remill_x86_set_segment_fs);
REMILL_DECL(__remill_x86_set_segment_gs);
REMILL_DECL(__remill_x86_set_segment_ss);

namespace aether {

const IntrinsicRaw HandlerIntrinsic[] = {
    REMILL_ITEM(__remill_aarch32_check_not_el2),
    REMILL_ITEM(__remill_aarch32_emulate_instruction),
    REMILL_ITEM(__remill_aarch64_emulate_instruction),
    REMILL_ITEM(__remill_amd64_set_control_reg_0),
    REMILL_ITEM(__remill_amd64_set_control_reg_1),
    REMILL_ITEM(__remill_amd64_set_control_reg_2),
    REMILL_ITEM(__remill_amd64_set_control_reg_3),
    REMILL_ITEM(__remill_amd64_set_control_reg_4),
    REMILL_ITEM(__remill_amd64_set_control_reg_8),
    REMILL_ITEM(__remill_amd64_set_debug_reg),
    REMILL_ITEM(__remill_async_hyper_call),
    REMILL_ITEM(__remill_atomic_begin),
    REMILL_ITEM(__remill_atomic_end),
    REMILL_ITEM(__remill_barrier_load_load),
    REMILL_ITEM(__remill_barrier_load_store),
    REMILL_ITEM(__remill_barrier_store_load),
    REMILL_ITEM(__remill_barrier_store_store),
    REMILL_ITEM(__remill_compare_eq),
    REMILL_ITEM(__remill_compare_exchange_memory_128),
    REMILL_ITEM(__remill_compare_exchange_memory_16),
    REMILL_ITEM(__remill_compare_exchange_memory_32),
    REMILL_ITEM(__remill_compare_exchange_memory_64),
    REMILL_ITEM(__remill_compare_exchange_memory_8),
    REMILL_ITEM(__remill_compare_neq),
    REMILL_ITEM(__remill_compare_sge),
    REMILL_ITEM(__remill_compare_sgt),
    REMILL_ITEM(__remill_compare_sle),
    REMILL_ITEM(__remill_compare_slt),
    REMILL_ITEM(__remill_compare_uge),
    REMILL_ITEM(__remill_compare_ugt),
    REMILL_ITEM(__remill_compare_ule),
    REMILL_ITEM(__remill_compare_ult),
    REMILL_ITEM(__remill_defer_inlining),
    REMILL_ITEM(__remill_delay_slot_begin),
    REMILL_ITEM(__remill_delay_slot_end),
    REMILL_ITEM(__remill_error),
    REMILL_ITEM(__remill_fetch_and_add_16),
    REMILL_ITEM(__remill_fetch_and_add_32),
    REMILL_ITEM(__remill_fetch_and_add_64),
    REMILL_ITEM(__remill_fetch_and_add_8),
    REMILL_ITEM(__remill_fetch_and_and_16),
    REMILL_ITEM(__remill_fetch_and_and_32),
    REMILL_ITEM(__remill_fetch_and_and_64),
    REMILL_ITEM(__remill_fetch_and_and_8),
    REMILL_ITEM(__remill_fetch_and_or_16),
    REMILL_ITEM(__remill_fetch_and_or_32),
    REMILL_ITEM(__remill_fetch_and_or_64),
    REMILL_ITEM(__remill_fetch_and_or_8),
    REMILL_ITEM(__remill_fetch_and_sub_16),
    REMILL_ITEM(__remill_fetch_and_sub_32),
    REMILL_ITEM(__remill_fetch_and_sub_64),
    REMILL_ITEM(__remill_fetch_and_sub_8),
    REMILL_ITEM(__remill_fetch_and_xor_16),
    REMILL_ITEM(__remill_fetch_and_xor_32),
    REMILL_ITEM(__remill_fetch_and_xor_64),
    REMILL_ITEM(__remill_fetch_and_xor_8),
    REMILL_ITEM(__remill_flag_computation_carry),
    REMILL_ITEM(__remill_flag_computation_overflow),
    REMILL_ITEM(__remill_flag_computation_sign),
    REMILL_ITEM(__remill_flag_computation_zero),
    REMILL_ITEM(__remill_fpu_exception_clear),
    REMILL_ITEM(__remill_fpu_exception_raise),
    REMILL_ITEM(__remill_fpu_exception_test),
    REMILL_ITEM(__remill_fpu_get_rounding),
    REMILL_ITEM(__remill_fpu_set_rounding),
    REMILL_ITEM(__remill_function_call),
    REMILL_ITEM(__remill_function_return),
    REMILL_ITEM(__remill_jump),
    REMILL_ITEM(__remill_mark_as_used),
    REMILL_ITEM(__remill_missing_block),
    REMILL_ITEM(__remill_read_io_port_16),
    REMILL_ITEM(__remill_read_io_port_32),
    REMILL_ITEM(__remill_read_io_port_8),
    REMILL_ITEM(__remill_read_memory_16),
    REMILL_ITEM(__remill_read_memory_32),
    REMILL_ITEM(__remill_read_memory_64),
    REMILL_ITEM(__remill_read_memory_8),
    REMILL_ITEM(__remill_read_memory_f128),
    REMILL_ITEM(__remill_read_memory_f32),
    REMILL_ITEM(__remill_read_memory_f64),
    REMILL_ITEM(__remill_read_memory_f80),
    REMILL_ITEM(__remill_sparc32_emulate_instruction),
    REMILL_ITEM(__remill_sparc64_emulate_instruction),
    REMILL_ITEM(__remill_sparc_set_asi_register),
    REMILL_ITEM(__remill_sparc_trap_cond_a),
    REMILL_ITEM(__remill_sparc_trap_cond_cc),
    REMILL_ITEM(__remill_sparc_trap_cond_cs),
    REMILL_ITEM(__remill_sparc_trap_cond_e),
    REMILL_ITEM(__remill_sparc_trap_cond_g),
    REMILL_ITEM(__remill_sparc_trap_cond_ge),
    REMILL_ITEM(__remill_sparc_trap_cond_gu),
    REMILL_ITEM(__remill_sparc_trap_cond_l),
    REMILL_ITEM(__remill_sparc_trap_cond_le),
    REMILL_ITEM(__remill_sparc_trap_cond_leu),
    REMILL_ITEM(__remill_sparc_trap_cond_n),
    REMILL_ITEM(__remill_sparc_trap_cond_ne),
    REMILL_ITEM(__remill_sparc_trap_cond_neg),
    REMILL_ITEM(__remill_sparc_trap_cond_pos),
    REMILL_ITEM(__remill_sparc_trap_cond_vc),
    REMILL_ITEM(__remill_sparc_trap_cond_vs),
    REMILL_ITEM(__remill_sparc_unhandled_dcti),
    REMILL_ITEM(__remill_sparc_unimplemented_instruction),
    REMILL_ITEM(__remill_sparc_window_underflow),
    REMILL_ITEM(__remill_sync_hyper_call),
    REMILL_ITEM(__remill_undefined_16),
    REMILL_ITEM(__remill_undefined_32),
    REMILL_ITEM(__remill_undefined_64),
    REMILL_ITEM(__remill_undefined_8),
    REMILL_ITEM(__remill_undefined_f128),
    REMILL_ITEM(__remill_undefined_f32),
    REMILL_ITEM(__remill_undefined_f64),
    REMILL_ITEM(__remill_undefined_f80),
    REMILL_ITEM(__remill_write_io_port_16),
    REMILL_ITEM(__remill_write_io_port_32),
    REMILL_ITEM(__remill_write_io_port_8),
    REMILL_ITEM(__remill_write_memory_16),
    REMILL_ITEM(__remill_write_memory_32),
    REMILL_ITEM(__remill_write_memory_64),
    REMILL_ITEM(__remill_write_memory_8),
    REMILL_ITEM(__remill_write_memory_f128),
    REMILL_ITEM(__remill_write_memory_f32),
    REMILL_ITEM(__remill_write_memory_f64),
    REMILL_ITEM(__remill_write_memory_f80),
    REMILL_ITEM(__remill_x86_set_control_reg_0),
    REMILL_ITEM(__remill_x86_set_control_reg_1),
    REMILL_ITEM(__remill_x86_set_control_reg_2),
    REMILL_ITEM(__remill_x86_set_control_reg_3),
    REMILL_ITEM(__remill_x86_set_control_reg_4),
    REMILL_ITEM(__remill_x86_set_debug_reg),
    REMILL_ITEM(__remill_x86_set_segment_ds),
    REMILL_ITEM(__remill_x86_set_segment_es),
    REMILL_ITEM(__remill_x86_set_segment_fs),
    REMILL_ITEM(__remill_x86_set_segment_gs),
    REMILL_ITEM(__remill_x86_set_segment_ss),
};

const size_t HandlerIntrinsicNum = std::size(HandlerIntrinsic);

} // namespace aether

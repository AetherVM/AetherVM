// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#define __remill_state __remill_state_x86

#define ISEL_UNSUPPORTED_INSTRUCTION ISEL_UNSUPPORTED_INSTRUCTION_X86
#define ISEL_INVALID_INSTRUCTION ISEL_INVALID_INSTRUCTION_X86

#include <cstdlib>

#if ANDROID
#include <sys/ucontext.h>

// Android's ucontext.h defines the following macros, which conflict with the
// remill x86 semantics code. Undefine them here to avoid conflicts.
#undef REG_IP
#undef REG_EIP
#undef REG_RIP
#undef REG_SP
#undef REG_ESP
#undef REG_RSP
#undef REG_BP
#undef REG_EBP
#undef REG_RBP
#undef REG_AL
#undef REG_AH
#undef REG_AX
#undef REG_EAX
#undef REG_RAX
#undef REG_BL
#undef REG_BH
#undef REG_BX
#undef REG_EBX
#undef REG_RBX
#undef REG_DL
#undef REG_DH
#undef REG_DX
#undef REG_EDX
#undef REG_RDX
#undef REG_CL
#undef REG_CH
#undef REG_CX
#undef REG_ECX
#undef REG_RCX
#undef REG_SIL
#undef REG_SI
#undef REG_ESI
#undef REG_RSI
#undef REG_DIL
#undef REG_DI
#undef REG_EDI
#undef REG_RDI
#undef REG_PC
#undef REG_XIP
#undef REG_XAX
#undef REG_XDX
#undef REG_XCX
#undef REG_XSI
#undef REG_XDI
#undef REG_XSP
#undef REG_XBP
#undef REG_XBX
#undef FLAG_CF
#undef FLAG_PF
#undef FLAG_AF
#undef FLAG_ZF
#undef FLAG_SF
#undef FLAG_OF
#undef FLAG_DF
#undef X87_ST0
#undef X87_ST1
#undef X87_ST2
#undef X87_ST3
#undef X87_ST4
#undef X87_ST5
#undef X87_ST6
#undef X87_ST7
#undef REG_SS
#undef REG_ES
#undef REG_DS
#undef REG_FS
#undef REG_GS
#undef REG_CS
#undef REG_SS_BASE
#undef REG_ES_BASE
#undef REG_DS_BASE
#undef REG_FS_BASE
#undef REG_GS_BASE
#undef REG_CS_BASE
#undef HYPER_CALL
#undef INTERRUPT_VECTOR
#endif // end of ANDROID

#include <lib/Arch/X86/Runtime/Instructions.cpp>

#define UNDEF_ISEL(n)                                                          \
  static void _##n(void) { abort(); }                                          \
  extern "C" void (*ISEL_##n)(void) = _##n

UNDEF_ISEL(BOUND_GPRv_MEMa16_16);

UNDEF_ISEL(BOUND_GPRv_MEMa32_32);

UNDEF_ISEL(CALL_NEAR_RELBRd_16);

UNDEF_ISEL(CALL_NEAR_RELBRd_32);

UNDEF_ISEL(CALL_NEAR_RELBRd_8);

UNDEF_ISEL(CALL_NEAR_RELBRz_16);

UNDEF_ISEL(CALL_NEAR_RELBRz_32);

UNDEF_ISEL(CALL_NEAR_RELBRz_8);

UNDEF_ISEL(INTO);

UNDEF_ISEL(IRETD_32);

UNDEF_ISEL(JMP_FAR_PTRp_IMMw_16);

UNDEF_ISEL(JMP_FAR_PTRp_IMMw_32);

UNDEF_ISEL(JMP_GPRv_16);

UNDEF_ISEL(JMP_GPRv_32);

UNDEF_ISEL(JMP_MEMv_16);

UNDEF_ISEL(JMP_MEMv_32);

UNDEF_ISEL(JMP_RELBRz_16);

UNDEF_ISEL(JMP_RELBRz_32);

UNDEF_ISEL(JMP_RELBRz_8);

UNDEF_ISEL(LEAVE_32);

UNDEF_ISEL(LEAVE_8);

UNDEF_ISEL(POPAD_32);

UNDEF_ISEL(POPA_32);

UNDEF_ISEL(POPFD);

UNDEF_ISEL(POP_DS_32);

UNDEF_ISEL(POP_ES_32);

UNDEF_ISEL(POP_FS_32);

UNDEF_ISEL(POP_GPRv_51_32);

UNDEF_ISEL(POP_GPRv_51_8);

UNDEF_ISEL(POP_GPRv_58_32);

UNDEF_ISEL(POP_GPRv_58_8);

UNDEF_ISEL(POP_GPRv_8F_32);

UNDEF_ISEL(POP_GPRv_8F_8);

UNDEF_ISEL(POP_GS_32);

UNDEF_ISEL(POP_MEM_XSP_32);

UNDEF_ISEL(POP_MEM_XSP_8);

UNDEF_ISEL(POP_MEMv_32);

UNDEF_ISEL(POP_MEMv_8);

UNDEF_ISEL(POP_SS_32);

UNDEF_ISEL(PUSHAD_32);

UNDEF_ISEL(PUSHA_16);

UNDEF_ISEL(PUSHFD);

UNDEF_ISEL(PUSH_GPRv_50_32);

UNDEF_ISEL(PUSH_GPRv_50_8);

UNDEF_ISEL(PUSH_GPRv_FFr6_32);

UNDEF_ISEL(PUSH_GPRv_FFr6_8);

UNDEF_ISEL(PUSH_IMMb_32);

UNDEF_ISEL(PUSH_IMMb_8);

UNDEF_ISEL(PUSH_IMMz_32);

UNDEF_ISEL(PUSH_IMMz_8);

UNDEF_ISEL(PUSH_MEMv_32);

UNDEF_ISEL(PUSH_MEMv_8);

UNDEF_ISEL(RET_NEAR_16);

UNDEF_ISEL(RET_NEAR_32);

UNDEF_ISEL(RET_NEAR_8);

UNDEF_ISEL(RET_NEAR_IMMw_16);

UNDEF_ISEL(RET_NEAR_IMMw_32);

UNDEF_ISEL(RET_NEAR_IMMw_8);

UNDEF_ISEL(AAS);

UNDEF_ISEL(CALL_NEAR_GPRv_16);

UNDEF_ISEL(CALL_NEAR_GPRv_32);

UNDEF_ISEL(CALL_NEAR_MEMv_16);

UNDEF_ISEL(CALL_NEAR_MEMv_32);

UNDEF_ISEL(DAA);

UNDEF_ISEL(ENTER_IMMw_IMMb_32);

UNDEF_ISEL(FSTPNCE_X87_ST0);

UNDEF_ISEL(VUNPCKLPD_YMMqq_YMMqq_MEMqq);

UNDEF_ISEL(VUNPCKLPD_YMMqq_YMMqq_YMMqq);

UNDEF_ISEL(VUNPCKLPS_YMMqq_YMMqq_MEMqq);

UNDEF_ISEL(VUNPCKLPS_YMMqq_YMMqq_YMMqq);

#include <generated/HandlerX86.cpp>

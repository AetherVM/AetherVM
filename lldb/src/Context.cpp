// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Context.h"
#include "Thread.h"

#include "Plugins/Process/Utility/RegisterContext_x86.h"
#include "Plugins/Process/Utility/lldb-x86-register-enums.h"
#include "Utility/ARM64_DWARF_Registers.h"
#include "Utility/ARM64_ehframe_Registers.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/RegisterValue.h"

namespace lldb_private {

using enum aether::Register;

aether::BinaryEngine *Engine = nullptr;

namespace arm64 {

constexpr aether::Register registers[] = {
    X0,  X1,  X2,  X3,  X4,  X5,   X6,  X7,  X8,  X9,  X10, X11, X12, X13,
    X14, X15, X16, X17, X18, X19,  X20, X21, X22, X23, X24, X25, X26, X27,
    X28, FP,  LR,  SP,  PC,  NZCV, Q0,  Q1,  Q2,  Q3,  Q4,  Q5,  Q6,  Q7,
    Q8,  Q9,  Q10, Q11, Q12, Q13,  Q14, Q15, Q16, Q17, Q18, Q19, Q20, Q21,
    Q22, Q23, Q24, Q25, Q26, Q27,  Q28, Q29, Q30, Q31,
};

#define GPR_OFFSET(reg) 0
#define FPU_OFFSET(reg) 0
#define GPR_OFFSET_NAME(reg) 0
#define FPR_OFFSET(reg) 0

// Generates register kinds array with DWARF, EH frame and generic kind
#define MISC_KIND(reg, type, generic_kind)                                     \
  {arm64_ehframe::reg,                                                         \
   arm64_dwarf::reg,                                                           \
   generic_kind,                                                               \
   LLDB_INVALID_REGNUM,                                                        \
   /*type##_##reg*/}

#define GPR64_KIND(reg, generic_kind) MISC_KIND(reg, gpr, generic_kind)
#define VREG_KIND(reg) MISC_KIND(reg, fpu, LLDB_INVALID_REGNUM)
#define MISC_GPR_KIND(lldb_kind) MISC_KIND(cpsr, gpr, LLDB_REGNUM_GENERIC_FLAGS)

// Defines a 64-bit general purpose register
#define DEFINE_GPR64(reg, generic_kind)                                        \
  {                                                                            \
      #reg,                                                                    \
      nullptr,                                                                 \
      8,                                                                       \
      GPR_OFFSET(gpr_##reg),                                                   \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      GPR64_KIND(reg, generic_kind),                                           \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

// Defines a 64-bit general purpose register
#define DEFINE_GPR64_ALT(reg, alt, generic_kind)                               \
  {                                                                            \
      #reg,                                                                    \
      #alt,                                                                    \
      8,                                                                       \
      GPR_OFFSET(gpr_##reg),                                                   \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      GPR64_KIND(reg, generic_kind),                                           \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

// Defines miscellaneous status and control registers like cpsr, fpsr etc
#define DEFINE_MISC_REGS(reg, size, TYPE, lldb_kind)                           \
  {                                                                            \
      #reg,                                                                    \
      nullptr,                                                                 \
      size,                                                                    \
      TYPE##_OFFSET_NAME(reg),                                                 \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      MISC_##TYPE##_KIND(lldb_kind),                                           \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

// Defines a vector register with 16-byte size
#define DEFINE_VREG(reg)                                                       \
  {                                                                            \
      #reg,                                                                    \
      nullptr,                                                                 \
      16,                                                                      \
      FPU_OFFSET(fpu_##reg - fpu_v0),                                          \
      lldb::eEncodingVector,                                                   \
      lldb::eFormatVectorOfUInt8,                                              \
      VREG_KIND(reg),                                                          \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

constexpr lldb_private::RegisterInfo register_infos[] = {
    // DEFINE_GPR64(name, GENERIC KIND)
    DEFINE_GPR64(x0, LLDB_REGNUM_GENERIC_ARG1),
    DEFINE_GPR64(x1, LLDB_REGNUM_GENERIC_ARG2),
    DEFINE_GPR64(x2, LLDB_REGNUM_GENERIC_ARG3),
    DEFINE_GPR64(x3, LLDB_REGNUM_GENERIC_ARG4),
    DEFINE_GPR64(x4, LLDB_REGNUM_GENERIC_ARG5),
    DEFINE_GPR64(x5, LLDB_REGNUM_GENERIC_ARG6),
    DEFINE_GPR64(x6, LLDB_REGNUM_GENERIC_ARG7),
    DEFINE_GPR64(x7, LLDB_REGNUM_GENERIC_ARG8),
    DEFINE_GPR64(x8, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x9, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x10, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x11, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x12, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x13, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x14, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x15, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x16, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x17, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x18, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x19, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x20, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x21, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x22, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x23, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x24, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x25, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x26, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x27, LLDB_INVALID_REGNUM),
    DEFINE_GPR64(x28, LLDB_INVALID_REGNUM),
    // DEFINE_GPR64(name, GENERIC KIND)
    DEFINE_GPR64_ALT(fp, x29, LLDB_REGNUM_GENERIC_FP),
    DEFINE_GPR64_ALT(lr, x30, LLDB_REGNUM_GENERIC_RA),
    DEFINE_GPR64_ALT(sp, x31, LLDB_REGNUM_GENERIC_SP),
    DEFINE_GPR64(pc, LLDB_REGNUM_GENERIC_PC),

    // DEFINE_MISC_REGS(name, size, TYPE, lldb kind)
    DEFINE_MISC_REGS(cpsr, 4, GPR, gpr_cpsr),

    // DEFINE_VREG(name)
    DEFINE_VREG(v0),
    DEFINE_VREG(v1),
    DEFINE_VREG(v2),
    DEFINE_VREG(v3),
    DEFINE_VREG(v4),
    DEFINE_VREG(v5),
    DEFINE_VREG(v6),
    DEFINE_VREG(v7),
    DEFINE_VREG(v8),
    DEFINE_VREG(v9),
    DEFINE_VREG(v10),
    DEFINE_VREG(v11),
    DEFINE_VREG(v12),
    DEFINE_VREG(v13),
    DEFINE_VREG(v14),
    DEFINE_VREG(v15),
    DEFINE_VREG(v16),
    DEFINE_VREG(v17),
    DEFINE_VREG(v18),
    DEFINE_VREG(v19),
    DEFINE_VREG(v20),
    DEFINE_VREG(v21),
    DEFINE_VREG(v22),
    DEFINE_VREG(v23),
    DEFINE_VREG(v24),
    DEFINE_VREG(v25),
    DEFINE_VREG(v26),
    DEFINE_VREG(v27),
    DEFINE_VREG(v28),
    DEFINE_VREG(v29),
    DEFINE_VREG(v30),
    DEFINE_VREG(v31),
};

static_assert(std::size(registers) == std::size(register_infos));

} // namespace arm64

namespace x86_64 {

enum {
  dwarf_xmm16_x86_64 = dwarf_xmm15_x86_64 + 1,
  dwarf_xmm17_x86_64,
  dwarf_xmm18_x86_64,
  dwarf_xmm19_x86_64,
  dwarf_xmm20_x86_64,
  dwarf_xmm21_x86_64,
  dwarf_xmm22_x86_64,
  dwarf_xmm23_x86_64,
  dwarf_xmm24_x86_64,
  dwarf_xmm25_x86_64,
  dwarf_xmm26_x86_64,
  dwarf_xmm27_x86_64,
  dwarf_xmm28_x86_64,
  dwarf_xmm29_x86_64,
  dwarf_xmm30_x86_64,
  dwarf_xmm31_x86_64,
};

enum {
  lldb_xmm16_x86_64 = lldb_xmm15_x86_64 + 1,
  lldb_xmm17_x86_64,
  lldb_xmm18_x86_64,
  lldb_xmm19_x86_64,
  lldb_xmm20_x86_64,
  lldb_xmm21_x86_64,
  lldb_xmm22_x86_64,
  lldb_xmm23_x86_64,
  lldb_xmm24_x86_64,
  lldb_xmm25_x86_64,
  lldb_xmm26_x86_64,
  lldb_xmm27_x86_64,
  lldb_xmm28_x86_64,
  lldb_xmm29_x86_64,
  lldb_xmm30_x86_64,
  lldb_xmm31_x86_64,
};

// see remill/Arch/X86/Runtime/State.h
#define FP_SIZE 10
#define XMM_SIZE 16

#define DEFINE_GPR(reg, alt, kind1, kind2, kind3, kind4)                       \
  {                                                                            \
      #reg,                                                                    \
      alt,                                                                     \
      8,                                                                       \
      GPR_OFFSET(reg),                                                         \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      {kind1, kind2, kind3, kind4, lldb_##reg##_x86_64},                       \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

#define DEFINE_FP_ST(reg, i)                                                   \
  {                                                                            \
      #reg #i,                                                                 \
      nullptr,                                                                 \
      FP_SIZE,                                                                 \
      LLVM_EXTENSION FPR_OFFSET(stmm[i]),                                      \
      lldb::eEncodingVector,                                                   \
      lldb::eFormatVectorOfUInt8,                                              \
      {dwarf_st##i##_x86_64, dwarf_st##i##_x86_64, LLDB_INVALID_REGNUM,        \
       LLDB_INVALID_REGNUM, lldb_st##i##_x86_64},                              \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

#define DEFINE_FP_MM(reg, i, streg)                                            \
  {                                                                            \
      #reg #i,                                                                 \
      nullptr,                                                                 \
      sizeof(uint64_t),                                                        \
      LLVM_EXTENSION FPR_OFFSET(stmm[i]),                                      \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      {dwarf_mm##i##_x86_64, dwarf_mm##i##_x86_64, LLDB_INVALID_REGNUM,        \
       LLDB_INVALID_REGNUM, lldb_mm##i##_x86_64},                              \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

#define DEFINE_XMM(reg, i)                                                     \
  {                                                                            \
      #reg #i,                                                                 \
      nullptr,                                                                 \
      XMM_SIZE,                                                                \
      LLVM_EXTENSION FPR_OFFSET(reg[i]),                                       \
      lldb::eEncodingVector,                                                   \
      lldb::eFormatVectorOfUInt8,                                              \
      {dwarf_##reg##i##_x86_64, dwarf_##reg##i##_x86_64, LLDB_INVALID_REGNUM,  \
       LLDB_INVALID_REGNUM, lldb_##reg##i##_x86_64},                           \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

constexpr aether::Register registers[] = {
    RIP,   RBP,   RSP,   RAX,   RBX,   RCX,   RDI,   RDX,    RSI,   R8,
    R9,    R10,   R11,   R12,   R13,   R14,   R15,   RFLAGS, ST0,   ST1,
    ST2,   ST3,   ST4,   ST5,   ST6,   ST7,   MM0,   MM1,    MM2,   MM3,
    MM4,   MM5,   MM6,   MM7,   XMM0,  XMM1,  XMM2,  XMM3,   XMM4,  XMM5,
    XMM6,  XMM7,  XMM8,  XMM9,  XMM10, XMM11, XMM12, XMM13,  XMM14, XMM15,
    XMM16, XMM17, XMM18, XMM19, XMM20, XMM21, XMM22, XMM23,  XMM24, XMM25,
    XMM26, XMM27, XMM28, XMM29, XMM30, XMM31,
};

constexpr RegisterInfo register_infos[] = {
    // General purpose registers  EH_Frame  DWARF Generic Process  Plugin
    DEFINE_GPR(rip, nullptr, dwarf_rip_x86_64, dwarf_rip_x86_64,
               LLDB_REGNUM_GENERIC_PC, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rbp, nullptr, dwarf_rbp_x86_64, dwarf_rbp_x86_64,
               LLDB_REGNUM_GENERIC_FP, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rsp, nullptr, dwarf_rsp_x86_64, dwarf_rsp_x86_64,
               LLDB_REGNUM_GENERIC_SP, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rax, nullptr, dwarf_rax_x86_64, dwarf_rax_x86_64,
               LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rbx, nullptr, dwarf_rbx_x86_64, dwarf_rbx_x86_64,
               LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rcx, nullptr, dwarf_rcx_x86_64, dwarf_rcx_x86_64,
               LLDB_REGNUM_GENERIC_ARG4, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rdi, nullptr, dwarf_rdi_x86_64, dwarf_rdi_x86_64,
               LLDB_REGNUM_GENERIC_ARG1, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rdx, nullptr, dwarf_rdx_x86_64, dwarf_rdx_x86_64,
               LLDB_REGNUM_GENERIC_ARG3, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rsi, nullptr, dwarf_rsi_x86_64, dwarf_rsi_x86_64,
               LLDB_REGNUM_GENERIC_ARG2, LLDB_INVALID_REGNUM),
    DEFINE_GPR(r8, nullptr, dwarf_r8_x86_64, dwarf_r8_x86_64,
               LLDB_REGNUM_GENERIC_ARG5, LLDB_INVALID_REGNUM),
    DEFINE_GPR(r9, nullptr, dwarf_r9_x86_64, dwarf_r9_x86_64,
               LLDB_REGNUM_GENERIC_ARG6, LLDB_INVALID_REGNUM),
    DEFINE_GPR(r10, nullptr, dwarf_r10_x86_64, dwarf_r10_x86_64,
               LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),
    DEFINE_GPR(r11, nullptr, dwarf_r11_x86_64, dwarf_r11_x86_64,
               LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),
    DEFINE_GPR(r12, nullptr, dwarf_r12_x86_64, dwarf_r12_x86_64,
               LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),
    DEFINE_GPR(r13, nullptr, dwarf_r13_x86_64, dwarf_r13_x86_64,
               LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),
    DEFINE_GPR(r14, nullptr, dwarf_r14_x86_64, dwarf_r14_x86_64,
               LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),
    DEFINE_GPR(r15, nullptr, dwarf_r15_x86_64, dwarf_r15_x86_64,
               LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM),
    DEFINE_GPR(rflags, nullptr, dwarf_rflags_x86_64, dwarf_rflags_x86_64,
               LLDB_REGNUM_GENERIC_FLAGS, LLDB_INVALID_REGNUM),

    // FP registers.
    DEFINE_FP_ST(st, 0),
    DEFINE_FP_ST(st, 1),
    DEFINE_FP_ST(st, 2),
    DEFINE_FP_ST(st, 3),
    DEFINE_FP_ST(st, 4),
    DEFINE_FP_ST(st, 5),
    DEFINE_FP_ST(st, 6),
    DEFINE_FP_ST(st, 7),

    DEFINE_FP_MM(mm, 0, st0),
    DEFINE_FP_MM(mm, 1, st1),
    DEFINE_FP_MM(mm, 2, st2),
    DEFINE_FP_MM(mm, 3, st3),
    DEFINE_FP_MM(mm, 4, st4),
    DEFINE_FP_MM(mm, 5, st5),
    DEFINE_FP_MM(mm, 6, st6),
    DEFINE_FP_MM(mm, 7, st7),

    // XMM registers
    DEFINE_XMM(xmm, 0),
    DEFINE_XMM(xmm, 1),
    DEFINE_XMM(xmm, 2),
    DEFINE_XMM(xmm, 3),
    DEFINE_XMM(xmm, 4),
    DEFINE_XMM(xmm, 5),
    DEFINE_XMM(xmm, 6),
    DEFINE_XMM(xmm, 7),
    DEFINE_XMM(xmm, 8),
    DEFINE_XMM(xmm, 9),
    DEFINE_XMM(xmm, 10),
    DEFINE_XMM(xmm, 11),
    DEFINE_XMM(xmm, 12),
    DEFINE_XMM(xmm, 13),
    DEFINE_XMM(xmm, 14),
    DEFINE_XMM(xmm, 15),
    DEFINE_XMM(xmm, 16),
    DEFINE_XMM(xmm, 17),
    DEFINE_XMM(xmm, 18),
    DEFINE_XMM(xmm, 19),
    DEFINE_XMM(xmm, 20),
    DEFINE_XMM(xmm, 21),
    DEFINE_XMM(xmm, 22),
    DEFINE_XMM(xmm, 23),
    DEFINE_XMM(xmm, 24),
    DEFINE_XMM(xmm, 25),
    DEFINE_XMM(xmm, 26),
    DEFINE_XMM(xmm, 27),
    DEFINE_XMM(xmm, 28),
    DEFINE_XMM(xmm, 29),
    DEFINE_XMM(xmm, 30),
    DEFINE_XMM(xmm, 31),
};

static_assert(std::size(registers) == std::size(register_infos));

} // namespace x86_64

void *AetherRegisterContext::GetCPU() const {
  return reinterpret_cast<AetherThread *>(&m_thread)->GetCPU();
}

uint32_t AetherRegisterContextARM64::GetRegisterCount() const {
  return std::size(arm64::registers);
}

uint32_t AetherRegisterContextX64::GetRegisterCount() const {
  return std::size(x86_64::registers);
}

const RegisterInfo *
AetherRegisterContextARM64::GetRegisterInfoAtIndex(uint32_t reg_index) const {
  return reg_index < std::size(arm64::registers)
             ? &arm64::register_infos[reg_index]
             : nullptr;
}

const RegisterInfo *
AetherRegisterContextX64::GetRegisterInfoAtIndex(uint32_t reg_index) const {
  return reg_index < std::size(x86_64::registers)
             ? &x86_64::register_infos[reg_index]
             : nullptr;
}

Status AetherRegisterContext::DoReadRegister(const RegisterInfo *reg_info,
                                             const RegisterInfo *reg_infos,
                                             const aether::Register *registers,
                                             RegisterValue &reg_value) {
  auto index = reg_info - reg_infos;
  auto reg = registers[index];
  auto regptr = Engine->getRegister(GetCPU(), reg);
  if (reg_info->byte_size <= 8)
    reg_value.SetUInt64(regptr->u8);
  else
    reg_value.SetBytes(regptr, reg_info->byte_size, endian::InlHostByteOrder());
  return Status();
}

Status AetherRegisterContextARM64::ReadRegister(const RegisterInfo *reg_info,
                                                RegisterValue &reg_value) {
  return DoReadRegister(reg_info, &arm64::register_infos[0],
                        &arm64::registers[0], reg_value);
}

Status AetherRegisterContextX64::ReadRegister(const RegisterInfo *reg_info,
                                              RegisterValue &reg_value) {
  return DoReadRegister(reg_info, &x86_64::register_infos[0],
                        &x86_64::registers[0], reg_value);
}

} // namespace lldb_private

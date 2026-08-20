// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "BinaryEngine.h"
#include <Platform.h>

namespace aether {

thread_local CPUState CPU;

bool CPUState::initContext(addr_t entry) {
  // init stack buffer if necessary
  if (!stack) {
    stacksize = stack_size();
    if (!(stack = new char[stacksize]))
      return false;
  }

  auto setRegister = [this](Register reg, RegisterValue val) {
    runtime->arch == ARM64 ? setRegisterAArch64(reg, val)
                           : setRegisterX86(reg, val);
  };
  auto getRegister = [this](Register reg) {
    return runtime->arch == ARM64 ? getRegisterAArch64(reg)
                                  : getRegisterX86(reg);
  };
  RegisterValue pc{.u8 = entry};
  RegisterValue sp{.ptr = stack + stacksize};
  // reset SP and PC
  setRegister(Register::PC, pc);
  setRegister(Register::SP, sp);
  // cache pc location to allow fast access during executing
  pcptr = reinterpret_cast<uintptr_t *>(
      const_cast<RegisterValue *>(getRegister(Register::PC)));

  if (runtime->eventConf.debug)
    runtime->dbgContext.thread_handler(this);
  return true;
}

void CPUState::freeContext() {
  if (runtime->eventConf.debug)
    runtime->dbgContext.thread_handler(nullptr);

  delete[] stack;
  stack = nullptr;
  runtime = nullptr;
}

const RegisterValue *CPUState::getRegisterAArch64(Register reg) {
  const void *ptr = nullptr;
  using enum Register;
  switch (reg) {
  case PC:
    ptr = &aarch64.gpr.pc;
    break;
  case X0:
    ptr = &aarch64.gpr.x0;
    break;
  case X1:
    ptr = &aarch64.gpr.x1;
    break;
  case X2:
    ptr = &aarch64.gpr.x2;
    break;
  case X3:
    ptr = &aarch64.gpr.x3;
    break;
  case X4:
    ptr = &aarch64.gpr.x4;
    break;
  case X5:
    ptr = &aarch64.gpr.x5;
    break;
  case X6:
    ptr = &aarch64.gpr.x6;
    break;
  case X7:
    ptr = &aarch64.gpr.x7;
    break;
  case X8:
    ptr = &aarch64.gpr.x8;
    break;
  case X9:
    ptr = &aarch64.gpr.x9;
    break;
  case X10:
    ptr = &aarch64.gpr.x10;
    break;
  case X11:
    ptr = &aarch64.gpr.x11;
    break;
  case X12:
    ptr = &aarch64.gpr.x12;
    break;
  case X13:
    ptr = &aarch64.gpr.x13;
    break;
  case X14:
    ptr = &aarch64.gpr.x14;
    break;
  case X15:
    ptr = &aarch64.gpr.x15;
    break;
  case X16:
    ptr = &aarch64.gpr.x16;
    break;
  case X17:
    ptr = &aarch64.gpr.x17;
    break;
  case X18:
    ptr = &aarch64.gpr.x18;
    break;
  case X19:
    ptr = &aarch64.gpr.x19;
    break;
  case X20:
    ptr = &aarch64.gpr.x20;
    break;
  case X21:
    ptr = &aarch64.gpr.x21;
    break;
  case X22:
    ptr = &aarch64.gpr.x22;
    break;
  case X23:
    ptr = &aarch64.gpr.x23;
    break;
  case X24:
    ptr = &aarch64.gpr.x24;
    break;
  case X25:
    ptr = &aarch64.gpr.x25;
    break;
  case X26:
    ptr = &aarch64.gpr.x26;
    break;
  case X27:
    ptr = &aarch64.gpr.x27;
    break;
  case X28:
    ptr = &aarch64.gpr.x28;
    break;
  case X29:
    ptr = &aarch64.gpr.x29;
    break;
  case X30:
    ptr = &aarch64.gpr.x30;
    break;
  case X31:
    ptr = &aarch64.gpr.sp;
    break;
  case NZCV:
    ptr = &aarch64.nzcv;
    break;
  case Q0:
  case Q1:
  case Q2:
  case Q3:
  case Q4:
  case Q5:
  case Q6:
  case Q7:
  case Q8:
  case Q9:
  case Q10:
  case Q11:
  case Q12:
  case Q13:
  case Q14:
  case Q15:
  case Q16:
  case Q17:
  case Q18:
  case Q19:
  case Q20:
  case Q21:
  case Q22:
  case Q23:
  case Q24:
  case Q25:
  case Q26:
  case Q27:
  case Q28:
  case Q29:
  case Q30:
  case Q31:
    ptr = &aarch64.simd.v[(int)reg - (int)Q0];
    break;
  default:
    return nullptr;
  }
  return reinterpret_cast<const RegisterValue *>(ptr);
}

bool CPUState::setRegisterAArch64(Register reg, RegisterValue val) {
  auto ptr = const_cast<RegisterValue *>(getRegisterAArch64(reg));
  if (!ptr)
    return false;

  *ptr = val;
  return true;
}

const RegisterValue *CPUState::getRegisterX86(Register reg) {
  const void *ptr = nullptr;
  using enum Register;
  switch (reg) {
  case RIP:
    ptr = &x86.gpr.rip;
    break;
  case RAX:
    ptr = &x86.gpr.rax;
    break;
  case RBP:
    ptr = &x86.gpr.rbp;
    break;
  case RBX:
    ptr = &x86.gpr.rbx;
    break;
  case RCX:
    ptr = &x86.gpr.rcx;
    break;
  case RDI:
    ptr = &x86.gpr.rdi;
    break;
  case RDX:
    ptr = &x86.gpr.rdx;
    break;
  case RSI:
    ptr = &x86.gpr.rsi;
    break;
  case RSP:
    ptr = &x86.gpr.rsp;
    break;
  case R8:
    ptr = &x86.gpr.r8;
    break;
  case R9:
    ptr = &x86.gpr.r9;
    break;
  case R10:
    ptr = &x86.gpr.r10;
    break;
  case R11:
    ptr = &x86.gpr.r11;
    break;
  case R12:
    ptr = &x86.gpr.r12;
    break;
  case R13:
    ptr = &x86.gpr.r13;
    break;
  case R14:
    ptr = &x86.gpr.r14;
    break;
  case R15:
    ptr = &x86.gpr.r15;
    break;
  case RFLAGS:
    ptr = &x86.rflag;
    break;
  case ST0:
  case ST1:
  case ST2:
  case ST3:
  case ST4:
  case ST5:
  case ST6:
  case ST7:
    ptr = &x86.st.elems[(int)reg - (int)ST0];
    break;
  case MM0:
  case MM1:
  case MM2:
  case MM3:
  case MM4:
  case MM5:
  case MM6:
  case MM7:
    ptr = &x86.mmx.elems[(int)reg - (int)MM0];
    break;
  case XMM0:
  case XMM1:
  case XMM2:
  case XMM3:
  case XMM4:
  case XMM5:
  case XMM6:
  case XMM7:
  case XMM8:
  case XMM9:
  case XMM10:
  case XMM11:
  case XMM12:
  case XMM13:
  case XMM14:
  case XMM15:
  case XMM16:
  case XMM17:
  case XMM18:
  case XMM19:
  case XMM20:
  case XMM21:
  case XMM22:
  case XMM23:
  case XMM24:
  case XMM25:
  case XMM26:
  case XMM27:
  case XMM28:
  case XMM29:
  case XMM30:
  case XMM31:
    ptr = &x86.vec[(int)reg - (int)XMM0];
    break;
  default:
    return nullptr;
  }
  return reinterpret_cast<const RegisterValue *>(ptr);
}

bool CPUState::setRegisterX86(Register reg, RegisterValue val) {
  auto ptr = const_cast<RegisterValue *>(getRegisterX86(reg));
  if (!ptr)
    return false;

  *ptr = val;
  return true;
}

} // namespace aether

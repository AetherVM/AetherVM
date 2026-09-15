// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

/*
AetherVM x86-64 ABI
────────────────────────

R12     CPU*
R13     Instruction*
R10     next handler

CPU[-0x10]   VM PC*

vm_enter:
    host → VM

handler:
    execute instruction
    R10 = next handler
    jmp R10

event:
    VM → host callback
    callback returns Instruction*
    resume VM
*/

#include "X86.h"

#include <charconv>

#if AETHER_ARCH_X64

// during the chained execution of the vm handlers:
// r12 is "void *cpu"
// r13 is 'const Instruction *insns'
// [r12-0x10] is pc pointer
// this event handler trampoline does:
// 1.call the real event handler to decide the next instruction to execute;
// 2.construct the right ABI(defined in remill/BC/ABI.h) to run;
#define IMPL_EVENT_VM_IMPL(n)                                                  \
  AETHER_ASM("mov %r12, %" ARGREG_0 "\n"                                       \
             "mov $0, %" ARGREG_1 "\n"                                         \
             "mov %r13, %" ARGREG_2 "\n"                                       \
             "call " HOST_CALL_PREFIX "host_" n "\n"                           \
             "mov %rax, %r13\n"                                                \
             "mov %r12, %" ARGREG_0 "\n"                                       \
             "mov -0x10(%r12), %" ARGREG_1 "\n"                                \
             "mov 0x0(%" ARGREG_1 "), %" ARGREG_1 "\n"                         \
             "mov %r13, %" ARGREG_2 "\n"                                       \
             "" extract_handler_r10 "")

#if AETHER_OS_WINDOWS
#define IMPL_EVENT_VM(n)                                                       \
  AETHER_NAKED void n(void) {                                                  \
    /* 0x20 shadow space + 0x08 alignment */                                   \
    AETHER_ASM("sub $0x28, %rsp\n");                                           \
    IMPL_EVENT_VM_IMPL(#n);                                                    \
    AETHER_ASM("add $0x28, %rsp\n"                                             \
               "jmp *%r10");                                                   \
  }
#else
#define IMPL_EVENT_VM(n)                                                       \
  AETHER_NAKED void n(void) {                                                  \
    /* stack alignment */                                                      \
    AETHER_ASM("push %rbp\n");                                                 \
    IMPL_EVENT_VM_IMPL(#n);                                                    \
    AETHER_ASM("pop %rbp\n"                                                    \
               "jmp *%r10");                                                   \
  }
#endif

IMPL_EVENT_VM(event_func_before);
IMPL_EVENT_VM(event_func_after);
IMPL_EVENT_VM(event_insn_before);
IMPL_EVENT_VM(event_insn_after);
IMPL_EVENT_VM(event_block_before);
IMPL_EVENT_VM(event_block_after);
IMPL_EVENT_VM(event_debugging);
IMPL_EVENT_VM(syscall_interpret);
IMPL_EVENT_VM(interrupt_interpret);
IMPL_EVENT_VM(jump_interpret);
IMPL_EVENT_VM(call_interpret);
IMPL_EVENT_VM(finish_function);
IMPL_EVENT_VM(finish_emulation);
IMPL_EVENT_VM(terminate_execution);
#endif // end of AETHER_ARCH_X64

namespace aether {

namespace x86 {

#if AETHER_ARCH_X64

extern "C" AETHER_NAKED void vm_enter_x64(void) {
  AETHER_ASM(
      // get the return address
      "mov 0x0(%rsp), %r10\n"
      // save the return address flag to cpu context
      "mov %r10, 0x0(%" ARGREG_3 ")\n"
      // set the return address to vm context
      "mov %r10, 0x0(%rax)\n"
      // get the real handler address
      "" extract_handler_r10 ""
      // ABI defined in remill/BC/ABI.h
      // call handler(state, vmaddr, memory)
      "jmp *%r10\n");
}

AETHER_VM_ENTRY() {
  AETHER_ASM(
#if AETHER_OS_WINDOWS
      /*
      On Windows x64, every caller must allocate 32 bytes (0x20) of stack
      space before making a function call, regardless of how many parameters
      are passed. This space is reserved directly at the top of the stack
      (rsp + 0x00 through rsp + 0x1F). Plus another 8 bytes for the return
      address, so the fifth argument is passed on the stack at [rsp + 0x20 +
      0x8].
      */
      "mov 0x28(%rsp), %rax\n"
#else
      "mov %" ARGREG_4 ", %rax\n"
#endif
      /*
      Argument ABI:
      REGARG_0: void *cpu
      REGARG_1: addr_t vmaddr
      REGARG_2: const Instruction *insns
      REGARG_3: uintptr_t *host_retaddr
      rax: void *(*vm_retaddr)()
      */
      "push %rbp\n"
      "push %r12\n"
      "push %r13\n"
      // The extra runtime context of vm handlers:
      // r12: cpu
      // r13: insns
      "mov %" ARGREG_0 ", %r12\n"
      "mov %" ARGREG_2 ", %r13\n"
      "push %" ARGREG_1 "\n" // save vmaddr
      "push %" ARGREG_3 "\n" // save host_retaddr
#if AETHER_OS_WINDOWS
      // shadow space (0x20)
      "sub $0x20, %rsp\n"
#endif
      "call *%rax\n"
#if AETHER_OS_WINDOWS
      "add $0x20, %rsp\n"
#endif
      "pop %" ARGREG_3 "\n" // restore host_retaddr
      "pop %" ARGREG_1 "\n" // restore vmaddr
      "mov %r12, %" ARGREG_0 "\n"
      "mov %r13, %" ARGREG_2 "\n"
#if AETHER_OS_WINDOWS
      "sub $0x20, %rsp\n"
#endif
      "call " HOST_CALL_PREFIX "vm_enter_x64\n"
#if AETHER_OS_WINDOWS
      // popup the return address and the shadow space
      "add $0x28, %rsp\n"
#else
      // popup the return address of calling vm_enter_x64
      "add $0x8, %rsp\n"
#endif
      "pop %r13\n"
      "pop %r12\n"
      "pop %rbp\n"
      "ret");
}

#else

AETHER_VM_ENTRY() { AETHER_ASM("brk #0"); }

#endif // end of AETHER_ARCH_X64

size_t offset_reg(Register reg) {
  // see remill/Arch/X86/Runtime/State.h for details
  State *state = nullptr;
  using enum Register;
  switch (reg) {
  case RIP:
    return (size_t)(size_t)&state->gpr.rip;
  case RAX:
    return (size_t)(size_t)&state->gpr.rax;
  case RBP:
    return (size_t)(size_t)&state->gpr.rbp;
  case RBX:
    return (size_t)(size_t)&state->gpr.rbx;
  case RCX:
    return (size_t)(size_t)&state->gpr.rcx;
  case RDI:
    return (size_t)(size_t)&state->gpr.rdi;
  case RDX:
    return (size_t)(size_t)&state->gpr.rdx;
  case RSI:
    return (size_t)(size_t)&state->gpr.rsi;
  case RSP:
    return (size_t)(size_t)&state->gpr.rsp;
  case R8:
    return (size_t)(size_t)&state->gpr.r8;
  case R9:
    return (size_t)(size_t)&state->gpr.r9;
  case R10:
    return (size_t)(size_t)&state->gpr.r10;
  case R11:
    return (size_t)(size_t)&state->gpr.r11;
  case R12:
    return (size_t)(size_t)&state->gpr.r12;
  case R13:
    return (size_t)(size_t)&state->gpr.r13;
  case R14:
    return (size_t)(size_t)&state->gpr.r14;
  case R15:
    return (size_t)(size_t)&state->gpr.r15;
  default:
    break;
  }
  static_assert(sizeof(state->st.elems[0]) == 0x10,
                "ST register size mismatch");
  static_assert(sizeof(state->mmx.elems[0]) == 0x10,
                "MMX register size mismatch");
  static_assert(sizeof(state->vec[0]) == 0x40, "Vector register size mismatch");
  if (ST0 <= reg && reg <= ST7)
    return (size_t)&state->st.elems[0] + 0x10 * ((int)reg - (int)ST0);
  if (MM0 <= reg && reg <= MM7)
    return (size_t)&state->mmx.elems[0] + 0x10 * ((int)reg - (int)MM0);
  if (XMM0 <= reg && reg <= XMM31)
    return (size_t)&state->vec[0].xmm + 0x40 * ((int)reg - (int)XMM0);
  abort();
}

size_t offset_reg(std::string_view reg) {
  State *state = nullptr;

  // 1. General Purpose Registers & Sub-register Aliases
  // GPR layout: volatile uint64_t _N followed by Reg field (stride of 0x10
  // bytes)
  static const std::pair<std::string_view, size_t> gpr_map[] = {
      {"RAX", (size_t)&state->gpr.rax}, {"EAX", (size_t)&state->gpr.rax},
      {"AX", (size_t)&state->gpr.rax},  {"AL", (size_t)&state->gpr.rax},
      {"AH", (size_t)&state->gpr.rax},  {"RBX", (size_t)&state->gpr.rbx},
      {"EBX", (size_t)&state->gpr.rbx}, {"BX", (size_t)&state->gpr.rbx},
      {"BL", (size_t)&state->gpr.rbx},  {"BH", (size_t)&state->gpr.rbx},
      {"RCX", (size_t)&state->gpr.rcx}, {"ECX", (size_t)&state->gpr.rcx},
      {"CX", (size_t)&state->gpr.rcx},  {"CL", (size_t)&state->gpr.rcx},
      {"CH", (size_t)&state->gpr.rcx},  {"RDX", (size_t)&state->gpr.rdx},
      {"EDX", (size_t)&state->gpr.rdx}, {"DX", (size_t)&state->gpr.rdx},
      {"DL", (size_t)&state->gpr.rdx},  {"DH", (size_t)&state->gpr.rdx},
      {"RSI", (size_t)&state->gpr.rsi}, {"ESI", (size_t)&state->gpr.rsi},
      {"SI", (size_t)&state->gpr.rsi},  {"SIL", (size_t)&state->gpr.rsi},
      {"RDI", (size_t)&state->gpr.rdi}, {"EDI", (size_t)&state->gpr.rdi},
      {"DI", (size_t)&state->gpr.rdi},  {"DIL", (size_t)&state->gpr.rdi},
      {"RSP", (size_t)&state->gpr.rsp}, {"ESP", (size_t)&state->gpr.rsp},
      {"SP", (size_t)&state->gpr.rsp},  {"SPL", (size_t)&state->gpr.rsp},
      {"RBP", (size_t)&state->gpr.rbp}, {"EBP", (size_t)&state->gpr.rbp},
      {"BP", (size_t)&state->gpr.rbp},  {"BPL", (size_t)&state->gpr.rbp},
      {"RIP", (size_t)&state->gpr.rip}, {"EIP", (size_t)&state->gpr.rip},
      {"IP", (size_t)&state->gpr.rip},
  };

  for (const auto &[name, off] : gpr_map) {
    if (reg == name)
      return off;
  }

  // Numbered GPRs (R8 - R15 and sub-register variants)
  if (reg.starts_with('R')) {
    int num = 0;
    std::string_view num_sv = reg.substr(1);
    if (num_sv.ends_with('D') || num_sv.ends_with('W') ||
        num_sv.ends_with('B')) {
      num_sv.remove_suffix(1);
    }
    auto res =
        std::from_chars(num_sv.data(), num_sv.data() + num_sv.size(), num);
    if (res.ec == std::errc{} && num >= 8 && num <= 15) {
      return (size_t)(size_t)&state->gpr.r8 + (0x10 * (num - 8));
    }
  }

  // 2. Vector Registers (ZMM0-31, YMM0-31, XMM0-31)
  if (reg.starts_with("ZMM") || reg.starts_with("YMM") ||
      reg.starts_with("XMM")) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 3, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num < (int)kNumVecRegisters) {
      return (size_t)&state->vec[num];
    }
  }

  // 3. Opmask / Mask Registers (K0 - K7)
  if (reg.starts_with('K')) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 1, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 7) {
      return (size_t)&state->k_reg.elems[num];
    }
  }

  // 4. MMX Registers (MM0 - MM7)
  if (reg.starts_with("MM")) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 2, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 7) {
      return (size_t)&state->mmx.elems[num];
    }
  }

  // 5. x87 FPU Stack Registers (ST0 - ST7)
  if (reg.starts_with("ST")) {
    int num = 0;
    auto res = std::from_chars(reg.data() + 2, reg.data() + reg.size(), num);
    if (res.ec == std::errc{} && num >= 0 && num <= 7) {
      return (size_t)&state->st.elems[num];
    }
  }

  // 6. Arithmetic Flags
  if (reg == "CF")
    return (size_t)&state->aflag.cf;
  if (reg == "PF")
    return (size_t)&state->aflag.pf;
  if (reg == "AF")
    return (size_t)&state->aflag.af;
  if (reg == "ZF")
    return (size_t)&state->aflag.zf;
  if (reg == "SF")
    return (size_t)&state->aflag.sf;
  if (reg == "DF")
    return (size_t)&state->aflag.df;
  if (reg == "OF")
    return (size_t)&state->aflag.of;

  // 7. Full RFlags / EFlags
  if (reg == "RFLAGS" || reg == "EFLAGS" || reg == "FLAGS") {
    return (size_t)&state->rflag;
  }

  // 8. Segment Selectors & Segment Base Addresses
  if (reg == "SS")
    return (size_t)&state->seg.ss;
  if (reg == "ES")
    return (size_t)&state->seg.es;
  if (reg == "GS")
    return (size_t)&state->seg.gs;
  if (reg == "FS")
    return (size_t)&state->seg.fs;
  if (reg == "DS")
    return (size_t)&state->seg.ds;
  if (reg == "CS")
    return (size_t)&state->seg.cs;

  if (reg == "SS_BASE")
    return (size_t)&state->addr.ss_base;
  if (reg == "ES_BASE")
    return (size_t)&state->addr.es_base;
  if (reg == "GS_BASE")
    return (size_t)&state->addr.gs_base;
  if (reg == "FS_BASE")
    return (size_t)&state->addr.fs_base;
  if (reg == "DS_BASE")
    return (size_t)&state->addr.ds_base;
  if (reg == "CS_BASE")
    return (size_t)&state->addr.cs_base;

  // 9. FPU Status Word Flags
  if (reg == "FPU_C0")
    return (size_t)&state->sw.c0;
  if (reg == "FPU_C1")
    return (size_t)&state->sw.c1;
  if (reg == "FPU_C2")
    return (size_t)&state->sw.c2;
  if (reg == "FPU_C3")
    return (size_t)&state->sw.c3;
  if (reg == "FPU_PE")
    return (size_t)&state->sw.pe;
  if (reg == "FPU_UE")
    return (size_t)&state->sw.ue;
  if (reg == "FPU_OE")
    return (size_t)&state->sw.oe;
  if (reg == "FPU_ZE")
    return (size_t)&state->sw.ze;
  if (reg == "FPU_DE")
    return (size_t)&state->sw.de;
  if (reg == "FPU_IE")
    return (size_t)&state->sw.ie;
  if (reg == "FPU_SF")
    return (size_t)&state->sw.sf;

  abort();
}

} // namespace x86

} // namespace aether

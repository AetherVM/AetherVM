// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <BinaryEngine.h>
#include <Handler.h>
#include <Lifter.h>
#include <OpcodeEngine.h>
#include <Orchestrator.h>
#include <Platform.h>
#include <UtilsAArch64.h>

#include <llvm/IR/Instruction.h>
#include <llvm/MC/MCInst.h>

#define GET_REGINFO_ENUM
#include <Target/AArch64/AArch64GenRegisterInfo.inc>

// shortcuts for engine implementation stub
#define engine (CPU.runtime)

using RemillRegister = remill::Operand::Register;

// prebuilt callable opcode
extern const uint8_t arm64_native_start[];
extern const uint8_t arm64_native_end[];

// host to vm
extern const void **vm_opcode_chain_h2v_xs[];
extern const void **vm_opcode_chain_h2v_qs[];
// vm to host
extern const void **vm_opcode_chain_v2h_xs[];
extern const void **vm_opcode_chain_v2h_qs[];

namespace aether {

namespace aarch64 {

#define IMPL_OPCODE_CHAIN_STR_LDR(n)                                           \
  AETHER_NAKED void vm_opcode_chain_str_##n(void) {                            \
    AETHER_ASM("str " #n ", [sp, #-0x10]!\n"                                   \
               "" extract_handler_x30_pre ""                                   \
               "br x30\n");                                                    \
  }                                                                            \
  AETHER_NAKED void vm_opcode_chain_ldr_##n(void) {                            \
    AETHER_ASM("ldr " #n ", [sp], #0x10\n"                                     \
               "" extract_handler_x30_pre ""                                   \
               "br x30\n");                                                    \
  }

IMPL_OPCODE_CHAIN_STR_LDR(x19);
IMPL_OPCODE_CHAIN_STR_LDR(x20);
IMPL_OPCODE_CHAIN_STR_LDR(x21);
IMPL_OPCODE_CHAIN_STR_LDR(x22);
IMPL_OPCODE_CHAIN_STR_LDR(x23);
IMPL_OPCODE_CHAIN_STR_LDR(x24);
IMPL_OPCODE_CHAIN_STR_LDR(x25);
IMPL_OPCODE_CHAIN_STR_LDR(x26);
IMPL_OPCODE_CHAIN_STR_LDR(x27);
IMPL_OPCODE_CHAIN_STR_LDR(x28);
IMPL_OPCODE_CHAIN_STR_LDR(x29);
IMPL_OPCODE_CHAIN_STR_LDR(x30);

IMPL_OPCODE_CHAIN_STR_LDR(d8);
IMPL_OPCODE_CHAIN_STR_LDR(d9);
IMPL_OPCODE_CHAIN_STR_LDR(d10);
IMPL_OPCODE_CHAIN_STR_LDR(d11);
IMPL_OPCODE_CHAIN_STR_LDR(d12);
IMPL_OPCODE_CHAIN_STR_LDR(d13);
IMPL_OPCODE_CHAIN_STR_LDR(d14);
IMPL_OPCODE_CHAIN_STR_LDR(d15);

#undef IMPL_OPCODE_CHAIN_STR_LDR
#define IMPL_OPCODE_CHAIN_STR_LDR(n) (void *)&vm_opcode_chain_str_##n

const void *vm_opcode_chain_str_xs[] = {
    IMPL_OPCODE_CHAIN_STR_LDR(x19), IMPL_OPCODE_CHAIN_STR_LDR(x20),
    IMPL_OPCODE_CHAIN_STR_LDR(x21), IMPL_OPCODE_CHAIN_STR_LDR(x22),
    IMPL_OPCODE_CHAIN_STR_LDR(x23), IMPL_OPCODE_CHAIN_STR_LDR(x24),
    IMPL_OPCODE_CHAIN_STR_LDR(x25), IMPL_OPCODE_CHAIN_STR_LDR(x26),
    IMPL_OPCODE_CHAIN_STR_LDR(x27), IMPL_OPCODE_CHAIN_STR_LDR(x28),
    IMPL_OPCODE_CHAIN_STR_LDR(x29), IMPL_OPCODE_CHAIN_STR_LDR(x30),
};

const void *vm_opcode_chain_str_ds[] = {
    IMPL_OPCODE_CHAIN_STR_LDR(d8),  IMPL_OPCODE_CHAIN_STR_LDR(d9),
    IMPL_OPCODE_CHAIN_STR_LDR(d10), IMPL_OPCODE_CHAIN_STR_LDR(d11),
    IMPL_OPCODE_CHAIN_STR_LDR(d12), IMPL_OPCODE_CHAIN_STR_LDR(d13),
    IMPL_OPCODE_CHAIN_STR_LDR(d14), IMPL_OPCODE_CHAIN_STR_LDR(d15),
};

#undef IMPL_OPCODE_CHAIN_STR_LDR
#define IMPL_OPCODE_CHAIN_STR_LDR(n) (void *)&vm_opcode_chain_ldr_##n

const void *vm_opcode_chain_ldr_xs[] = {
    IMPL_OPCODE_CHAIN_STR_LDR(x19), IMPL_OPCODE_CHAIN_STR_LDR(x20),
    IMPL_OPCODE_CHAIN_STR_LDR(x21), IMPL_OPCODE_CHAIN_STR_LDR(x22),
    IMPL_OPCODE_CHAIN_STR_LDR(x23), IMPL_OPCODE_CHAIN_STR_LDR(x24),
    IMPL_OPCODE_CHAIN_STR_LDR(x25), IMPL_OPCODE_CHAIN_STR_LDR(x26),
    IMPL_OPCODE_CHAIN_STR_LDR(x27), IMPL_OPCODE_CHAIN_STR_LDR(x28),
    IMPL_OPCODE_CHAIN_STR_LDR(x29), IMPL_OPCODE_CHAIN_STR_LDR(x30),
};

const void *vm_opcode_chain_ldr_ds[] = {
    IMPL_OPCODE_CHAIN_STR_LDR(d8),  IMPL_OPCODE_CHAIN_STR_LDR(d9),
    IMPL_OPCODE_CHAIN_STR_LDR(d10), IMPL_OPCODE_CHAIN_STR_LDR(d11),
    IMPL_OPCODE_CHAIN_STR_LDR(d12), IMPL_OPCODE_CHAIN_STR_LDR(d13),
    IMPL_OPCODE_CHAIN_STR_LDR(d14), IMPL_OPCODE_CHAIN_STR_LDR(d15),
};

#define IMPL_OPCODE_CHAIN_SAVE_X26(n)                                          \
  AETHER_NAKED void vm_opcode_chain_save_x26_##n(void) {                       \
    AETHER_ASM("mov " #n ", x26\n"                                             \
               "" extract_handler_x30_pre ""                                   \
               "br x30\n");                                                    \
  }

IMPL_OPCODE_CHAIN_SAVE_X26(x0);
IMPL_OPCODE_CHAIN_SAVE_X26(x1);
IMPL_OPCODE_CHAIN_SAVE_X26(x2);
IMPL_OPCODE_CHAIN_SAVE_X26(x3);
IMPL_OPCODE_CHAIN_SAVE_X26(x4);
IMPL_OPCODE_CHAIN_SAVE_X26(x5);

#undef IMPL_OPCODE_CHAIN_SAVE_X26
#define IMPL_OPCODE_CHAIN_SAVE_X26(n) (void *)&vm_opcode_chain_save_x26_##n

const void *vm_opcode_chain_save_x26[] = {
    IMPL_OPCODE_CHAIN_SAVE_X26(x0), IMPL_OPCODE_CHAIN_SAVE_X26(x1),
    IMPL_OPCODE_CHAIN_SAVE_X26(x2), IMPL_OPCODE_CHAIN_SAVE_X26(x3),
    IMPL_OPCODE_CHAIN_SAVE_X26(x4), IMPL_OPCODE_CHAIN_SAVE_X26(x5),
};

static AETHER_NAKED void execute_prebuilt(void) {
  AETHER_ASM("" extract_handler_x30_pre ""
             "blr x30\n" // call the prebuilt opcode
             "" extract_handler_x30_pre ""
             "mov x2, x27\n" // argument instruction
             "br x30");
}

static AETHER_NAKED void finish_opchain(void) {
  AETHER_ASM("mov x0, x26\n"          // argument state
             "ldr x1, [x0, #-0x10]\n" // load pcptr
             "ldr x2, [x1]\n"         // load pc
             "add x2, x2, #0x4\n"     // next pc
             "str x2, [x1]\n"         // set new pc
             "mov x1, x2\n"           // argument vmaddr
                                      // advance to the next instruction
             "" extract_handler_x30_pre ""
             "mov x2, x27\n" // argument instruction
             "br x30");
}

struct OpcodeNativeImpl {
  uint32_t opc;
  uint32_t _ret; // unused

  static const OpcodeNativeImpl *prebuilt;
  static const size_t size;

  explicit OpcodeNativeImpl(uint32_t opcode) : opc{opcode} {}

  const void *callable(llvm::MCInst &inst) {
    for (unsigned i = 0; i < inst.getNumOperands(); i++) {
      auto &opr = inst.getOperand(i);
      if (opr.isReg()) {
        using namespace llvm;
        switch (opr.getReg()) {
        case AArch64::X27: // x27 is our chain pointer
        case AArch64::LR:  // lr will be rewritten when using blr
          return nullptr;
        default:
          break;
        }
      }
    }
    auto found = binary_search(prebuilt, size, *this);
    return is_exact(found, prebuilt, size, *this) ? found : nullptr;
  }

  auto operator<=>(const OpcodeNativeImpl &right) const {
    return opc <=> right.opc;
  }
  bool operator==(const OpcodeNativeImpl &right) const {
    return opc == right.opc;
  }
};

static_assert(sizeof(OpcodeNativeImpl) == 8);

const OpcodeNativeImpl *OpcodeNativeImpl::prebuilt =
    (OpcodeNativeImpl *)&arm64_native_start[0];
const size_t OpcodeNativeImpl::size =
    (arm64_native_end - arm64_native_start) / sizeof(OpcodeNativeImpl);

void setup_chains(std::vector<const void *> &chains, const llvm::MCInst &inst,
                  const void *prebuilt) {
  using namespace aarch64;
  auto regused = parse_regused(inst);
  // save host context
  for (auto r : regused) {
    if (Register::X19 <= r && r <= Register::X30)
      chains.push_back(vm_opcode_chain_str_xs[(int)r - (int)Register::X19]);
    else if (Register::Q8 <= r && r <= Register::Q15)
      chains.push_back(vm_opcode_chain_str_ds[(int)r - (int)Register::Q8]);
  }

  // just use the original x26 or find a unused gpr as our cpu context
  auto regcpu = regused.find(Register::X26) == regused.end()
                    ? (int)Register::X26
                    : (int)Register::X0;
  while (regused.find((Register)regcpu) != regused.end())
    regcpu++;
  regcpu -= (int)Register::X0;
  if (regcpu != 26)
    chains.push_back(vm_opcode_chain_save_x26[regcpu]);
  else
    regcpu = 6; // index 6 is for X26
  assert(regcpu <= 6);

  // load guest context
  for (auto r : regused) {
    if (Register::X0 <= r && r <= Register::X30)
      chains.push_back(
          vm_opcode_chain_v2h_xs[(int)r - (int)Register::X0][regcpu]);
    else if (Register::Q0 <= r && r <= Register::Q31)
      chains.push_back(
          vm_opcode_chain_v2h_qs[(int)r - (int)Register::Q0][regcpu]);
  }

  chains.push_back((void *)&execute_prebuilt);
  chains.push_back((void *)prebuilt);

  // save guest context
  for (auto r : regused) {
    if (Register::X0 <= r && r <= Register::X30)
      chains.push_back(
          vm_opcode_chain_h2v_xs[(int)r - (int)Register::X0][regcpu]);
    else if (Register::Q0 <= r && r <= Register::Q31)
      chains.push_back(
          vm_opcode_chain_h2v_qs[(int)r - (int)Register::Q0][regcpu]);
  }

  // load host context
  for (auto rit = regused.rbegin(), rend = regused.rend(); rit != rend; rit++) {
    auto r = *rit;
    if (Register::X19 <= r && r <= Register::X30)
      chains.push_back(vm_opcode_chain_ldr_xs[(int)r - (int)Register::X19]);
    else if (Register::Q8 <= r && r <= Register::Q15)
      chains.push_back(vm_opcode_chain_ldr_ds[(int)r - (int)Register::Q8]);
  }

  // update pc and finish emulation
  chains.push_back((void *)&finish_opchain);
  chains.push_back((void *)&finish_emulation);
}

} // namespace aarch64

template <> void OpcodeHandler<uint32_t>::initPrebuilt() {
  llvm::MCInst inst;
  auto oplen =
      engine->diser.disassemble((uint8_t *)&opcode, sizeof(opcode), inst);
  if (initMCInst(inst))
    return;

  type = OHT_Prebuit;
  if (!oplen) {
    impl = (void *)&abort;
    return;
  }

  using namespace aarch64;
  OpcodeNativeImpl tmp{opcode};
  auto callable = tmp.callable(inst);
  if (callable) {
    impl = callable;
    setup_chains(chains, inst, impl);
  } else {
    type = OHT_PrebuiltMapped;

    OpcodeRegisters opregs;
    tmp.opc = normalize_opcode(engine->diser, inst, opcode, opregs);
    callable = tmp.callable(inst);
    if (!callable) {
      // should never happen
      abort();
    }
    impl = callable;
    setup_chains(chains, inst, impl);

    auto x0 = (unsigned)Register::X0;
    auto q0 = (unsigned)Register::Q0;
    for (auto r : opregs.regmaps)
      gpr.push_back(
          std::make_pair((Register)(x0 + r.first), (Register)(x0 + r.second)));
    for (auto r : opregs.fpumaps)
      fpu.push_back(
          std::make_pair((Register)(q0 + r.first), (Register)(q0 + r.second)));
  }
}

} // namespace aether

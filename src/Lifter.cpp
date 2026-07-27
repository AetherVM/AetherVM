// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Lifter.h>
#include <format>
#include <remill/BC/ABI.h>
#include <remill/BC/Util.h>

namespace aether {

std::map<uint32_t, uintptr_t> arch64;
std::map<std::string, uintptr_t> x86;

Lifter::Lifter(remill::Arch *ptr, const llvm::Module *pre)
    : arch(ptr), prebuilt(pre), module("aethervm", pre->getContext()) {
  arch->PrepareModule(&module);
}

Lifter::~Lifter() {}

void Lifter::transform(std::span<const uint8_t> opcode) {
  std::string name{"aethervm_"};
  for (auto b : opcode)
    name += std::format("{:02x}", b);

  auto intrinsics = arch->GetInstrinsicTable();
  auto func = arch->DeclareLiftedFunction(name, &module);
  arch->InitializeEmptyLiftedFunction(func);

  auto state_ptr = remill::NthArgument(func, remill::kStatePointerArgNum);
  auto body = llvm::BasicBlock::Create(module.getContext(), "", func);
  if (auto entry_block = &(func->front())) {
    auto pc = remill::LoadProgramCounterArg(func);
    auto [next_pc_ref, next_pc_ref_type] =
        this->arch->DefaultLifter(*intrinsics)
            ->LoadRegAddress(entry_block, state_ptr,
                             remill::kNextPCVariableName);

    // Initialize `NEXT_PC`.
    (void)new llvm::StoreInst(pc, next_pc_ref, entry_block);

    // Branch to the first basic block.
    llvm::BranchInst::Create(body, entry_block);
  }

  remill::Instruction inst;
  std::ignore = arch->DecodeInstruction(
      0, {(char *)opcode.data(), (char *)opcode.data() + opcode.size()}, inst,
      arch->CreateInitialContext());
  auto lift_status = inst.GetLifter()->LiftIntoBlock(inst, body, state_ptr);
  auto tailcall = remill::kLiftedInstruction == lift_status
                      ? intrinsics->function_return
                      : intrinsics->error;
  remill::AddTerminatingTailCall(body, tailcall, *intrinsics);
}

} // namespace aether

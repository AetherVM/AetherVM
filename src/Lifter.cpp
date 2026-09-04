// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Lifter.h"
#include "Orchestrator.h"
#include <Platform.h>
#include <Register.h>
#include <Utils.h>

#include "llvm/IR/InlineAsm.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#define GET_REGINFO_ENUM
#include <Target/AArch64/AArch64GenRegisterInfo.inc>

#define GET_REGINFO_ENUM
#include <Target/X86/X86GenRegisterInfo.inc>

#include <remill/BC/ABI.h>
#include <remill/BC/Optimizer.h>
#include <remill/BC/Util.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

#define DUMP_MIDDLE_IROBJECT 0

namespace fs = std::filesystem;

namespace aether {

namespace aarch64 {

size_t offset_reg(Register reg);

std::set<aether::Register> parse_regused(const llvm::MCInst &inst) {
  using namespace llvm;
  std::set<uint8_t> xregs, qregs;
  for (unsigned i = 0; i < inst.getNumOperands(); i++) {
    auto opr = inst.getOperand(i);
    if (!opr.isReg())
      continue;
    auto reg = opr.getReg();
    if (reg == AArch64::WZR || reg == AArch64::XZR)
      continue;
    if (reg >= AArch64::W0 && reg <= AArch64::W30) {
      xregs.insert(reg - AArch64::W0);
    } else if (reg == AArch64::WSP) {
      xregs.insert(31);
    } else if (reg >= AArch64::W0_W1 && reg <= AArch64::W28_W29) {
      xregs.insert(reg - AArch64::W0_W1 + 0);
      xregs.insert(reg - AArch64::W0_W1 + 1);
    } else if (reg >= AArch64::X0 && reg <= AArch64::X28) {
      xregs.insert(reg - AArch64::X0);
    } else if (reg >= AArch64::X0_X1 && reg <= AArch64::X26_X27) {
      xregs.insert(reg - AArch64::X0_X1 + 0);
      xregs.insert(reg - AArch64::X0_X1 + 1);
    } else if (reg == AArch64::FP) {
      xregs.insert(29);
    } else if (reg == AArch64::LR) {
      xregs.insert(30);
    } else if (reg == AArch64::SP) {
      xregs.insert(31);
    } else if (reg >= AArch64::B0 && reg <= AArch64::B31) {
      qregs.insert(reg - AArch64::B0);
    } else if (reg >= AArch64::H0 && reg <= AArch64::H31) {
      qregs.insert(reg - AArch64::H0);
    } else if (reg >= AArch64::S0 && reg <= AArch64::S31) {
      qregs.insert(reg - AArch64::S0);
    } else if (reg >= AArch64::D0 && reg <= AArch64::D31) {
      qregs.insert(reg - AArch64::D0);
    } else if (reg >= AArch64::D0_D1 && reg <= AArch64::D30_D31) {
      qregs.insert(reg - AArch64::D0_D1 + 0);
      qregs.insert(reg - AArch64::D0_D1 + 1);
    } else if (reg >= AArch64::D0_D1_D2 && reg <= AArch64::D29_D30_D31) {
      qregs.insert(reg - AArch64::D0_D1_D2 + 0);
      qregs.insert(reg - AArch64::D0_D1_D2 + 1);
      qregs.insert(reg - AArch64::D0_D1_D2 + 2);
    } else if (reg >= AArch64::D0_D1_D2_D3 && reg <= AArch64::D28_D29_D30_D31) {
      qregs.insert(reg - AArch64::D0_D1_D2_D3 + 0);
      qregs.insert(reg - AArch64::D0_D1_D2_D3 + 1);
      qregs.insert(reg - AArch64::D0_D1_D2_D3 + 2);
      qregs.insert(reg - AArch64::D0_D1_D2_D3 + 3);
    } else if (reg >= AArch64::Q0 && reg <= AArch64::Q31) {
      qregs.insert(reg - AArch64::Q0);
    } else if (reg >= AArch64::Q0_Q1 && reg <= AArch64::Q30_Q31) {
      qregs.insert(reg - AArch64::Q0_Q1 + 0);
      qregs.insert(reg - AArch64::Q0_Q1 + 1);
    } else if (reg >= AArch64::Q0_Q1_Q2 && reg <= AArch64::Q29_Q30_Q31) {
      qregs.insert(reg - AArch64::Q0_Q1_Q2 + 0);
      qregs.insert(reg - AArch64::Q0_Q1_Q2 + 1);
      qregs.insert(reg - AArch64::Q0_Q1_Q2 + 2);
    } else if (reg >= AArch64::Q0_Q1_Q2_Q3 && reg <= AArch64::Q28_Q29_Q30_Q31) {
      qregs.insert(reg - AArch64::Q0_Q1_Q2_Q3 + 0);
      qregs.insert(reg - AArch64::Q0_Q1_Q2_Q3 + 1);
      qregs.insert(reg - AArch64::Q0_Q1_Q2_Q3 + 2);
      qregs.insert(reg - AArch64::Q0_Q1_Q2_Q3 + 3);
    } else if (reg >= AArch64::Z0 && reg <= AArch64::Z31) {
      qregs.insert(reg - AArch64::Z0);
    } else if (reg >= AArch64::Z0_Z1 && reg <= AArch64::Z30_Z31) {
      qregs.insert(reg - AArch64::Z0_Z1 + 0);
      qregs.insert(reg - AArch64::Z0_Z1 + 1);
    } else if (reg >= AArch64::Z0_Z1_Z2 && reg <= AArch64::Z29_Z30_Z31) {
      qregs.insert(reg - AArch64::Z0_Z1_Z2 + 0);
      qregs.insert(reg - AArch64::Z0_Z1_Z2 + 1);
      qregs.insert(reg - AArch64::Z0_Z1_Z2 + 2);
    } else if (reg >= AArch64::Z0_Z1_Z2_Z3 && reg <= AArch64::Z28_Z29_Z30_Z31) {
      qregs.insert(reg - AArch64::Z0_Z1_Z2_Z3 + 0);
      qregs.insert(reg - AArch64::Z0_Z1_Z2_Z3 + 1);
      qregs.insert(reg - AArch64::Z0_Z1_Z2_Z3 + 2);
      qregs.insert(reg - AArch64::Z0_Z1_Z2_Z3 + 3);
    }
  }
  std::set<aether::Register> regused;
  for (auto x : xregs)
    regused.insert((aether::Register)((uint8_t)Register::X0 + x));
  for (auto q : qregs)
    regused.insert((aether::Register)((uint8_t)Register::Q0 + q));
  return regused;
}

} // namespace aarch64

namespace x86 {

size_t offset_reg(Register reg);

aether::Register canonicalize(unsigned reg) {
  using namespace llvm;
  switch (reg) {
  case X86::RAX:
  case X86::EAX:
  case X86::AX:
  case X86::AL:
  case X86::AH:
    return Register::RAX;
  case X86::RBX:
  case X86::EBX:
  case X86::BX:
  case X86::BL:
  case X86::BH:
    return Register::RBX;
  case X86::RCX:
  case X86::ECX:
  case X86::CX:
  case X86::CL:
  case X86::CH:
    return Register::RCX;
  case X86::RDX:
  case X86::EDX:
  case X86::DX:
  case X86::DL:
  case X86::DH:
    return Register::RDX;
  case X86::RDI:
  case X86::EDI:
  case X86::DI:
  case X86::DIL:
    return Register::RDI;
  case X86::RSI:
  case X86::ESI:
  case X86::SI:
  case X86::SIL:
    return Register::RSI;
  case X86::RBP:
  case X86::EBP:
  case X86::BP:
  case X86::BPL:
    return Register::RBP;
  case X86::RSP:
  case X86::ESP:
  case X86::SP:
  case X86::SPL:
    return Register::RSP;
  case X86::R8:
  case X86::R8D:
  case X86::R8W:
  case X86::R8B:
    return Register::R8;
  case X86::R9:
  case X86::R9D:
  case X86::R9W:
  case X86::R9B:
    return Register::R9;
  case X86::R10:
  case X86::R10D:
  case X86::R10W:
  case X86::R10B:
    return Register::R10;
  case X86::R11:
  case X86::R11D:
  case X86::R11W:
  case X86::R11B:
    return Register::R11;
  case X86::R12:
  case X86::R12D:
  case X86::R12W:
  case X86::R12B:
    return Register::R12;
  case X86::R13:
  case X86::R13D:
  case X86::R13W:
  case X86::R13B:
    return Register::R13;
  case X86::R14:
  case X86::R14D:
  case X86::R14W:
  case X86::R14B:
    return Register::R14;
  case X86::R15:
  case X86::R15D:
  case X86::R15W:
  case X86::R15B:
    return Register::R15;
  default:
    abort();
  }
}

bool is_gpr64(Register r) {
  switch (r) {
  case Register::RAX:
  case Register::RBX:
  case Register::RCX:
  case Register::RDX:
  case Register::RDI:
  case Register::RSI:
  case Register::RSP:
  case Register::RBP:
  case Register::R8:
  case Register::R9:
  case Register::R10:
  case Register::R11:
  case Register::R12:
  case Register::R13:
  case Register::R14:
  case Register::R15:
    return true;
  default:
    return false;
  }
}

const char *gpr_name64(Register r) {
  switch (r) {
  case Register::RAX:
    return "rax";
  case Register::RBX:
    return "rbx";
  case Register::RCX:
    return "rcx";
  case Register::RDX:
    return "rdx";
  case Register::RDI:
    return "rdi";
  case Register::RSI:
    return "rsi";
  case Register::RSP:
    return "rsp";
  case Register::RBP:
    return "rbp";
  case Register::R8:
    return "r8";
  case Register::R9:
    return "r9";
  case Register::R10:
    return "r10";
  case Register::R11:
    return "r11";
  case Register::R12:
    return "r12";
  case Register::R13:
    return "r13";
  case Register::R14:
    return "r14";
  case Register::R15:
    return "r15";
  default:
    return "?";
  }
}

std::string xmm_name(Register r) {
  return std::format("xmm{}", (int)r - (int)Register::XMM0);
}

std::set<aether::Register> parse_regused(const llvm::MCInst &inst) {
  using namespace llvm;
  std::set<aether::Register> regused;
  for (unsigned i = 0; i < inst.getNumOperands(); i++) {
    auto opr = inst.getOperand(i);
    if (!opr.isReg())
      continue;
    auto reg = opr.getReg();
    if (reg == X86::NoRegister)
      continue;
    // Scope to XMM0-15 (legacy/AVX-128 encodings); the enum reserves up to
    // XMM31 for AVX-512, out of scope for this handler shape.
    if (reg >= X86::XMM0 && reg <= X86::XMM31) {
      regused.insert(
          (aether::Register)((int)Register::XMM0 + (reg - X86::XMM0)));
      continue;
    }
    regused.insert(x86::canonicalize(reg));
  }
  return regused;
}

} // namespace x86

namespace {

constexpr std::string_view dyn_prefix{"aethervm_"};

std::unique_ptr<llvm::MemoryBuffer> generate_object(llvm::Module &module) {
  auto triple = module.getTargetTriple();
  std::string errorStr;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple, errorStr);

  llvm::TargetOptions opt;
  std::string_view feature =
      triple.getArch() == llvm::Triple::aarch64 ? "+all" : "";
  auto targetMachine =
      std::unique_ptr<llvm::TargetMachine>(target->createTargetMachine(
          triple, "generic", feature, opt, llvm::Reloc::Static, std::nullopt,
          llvm::CodeGenOptLevel::Default));

  llvm::SmallVector<char, 0> buffer;
  llvm::raw_svector_ostream os(buffer);

  llvm::legacy::PassManager pm;
  targetMachine->addPassesToEmitFile(pm, os, nullptr,
                                     llvm::CodeGenFileType::ObjectFile);
  pm.run(module);

#if DUMP_MIDDLE_IROBJECT
  auto llpath = fs::temp_directory_path() / "aethervm.ll";
  std::error_code ec;
  llvm::raw_fd_ostream file(llpath.string(), ec,
                            llvm::sys::fs::OpenFlags::OF_Text);
  module.print(file, nullptr);
  log_print(Develop, "Saved in-memory bitcode {}.", llpath.string());

  auto objpath = fs::temp_directory_path() / "aethervm.obj";
  std::ofstream outf(objpath, std::ios::binary);
  outf.write(buffer.data(), buffer.size());
  outf.close();
  log_print(Develop, "Saved in-memory object {}.", objpath.string());
#endif

  return llvm::MemoryBuffer::getMemBufferCopy(
      llvm::StringRef(buffer.data(), buffer.size()), "aethervm_object");
}

void create_trampoline(llvm::Module &M, llvm::Function *TargetFn,
                       uint64_t targetAddress) {
  std::string TempName{dyn_prefix};
  TempName += TargetFn->getName();
  if (llvm::Function *TrampolineFn = M.getFunction(TempName)) {
    // already exists
    TargetFn->replaceAllUsesWith(TrampolineFn);
    return;
  }

  llvm::LLVMContext &Ctx = M.getContext();
  llvm::FunctionType *FnTy = TargetFn->getFunctionType();
  llvm::Function *TrampolineFn = llvm::Function::Create(
      FnTy, llvm::GlobalValue::ExternalLinkage, TempName, &M);
  // don't let optimizer inline our trampoline function, the internal reference
  // relocations will be resolved later
  TrampolineFn->addFnAttr(llvm::Attribute::NoInline);

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(Ctx, "entry", TrampolineFn);
  llvm::IRBuilder<> Builder(BB);
  llvm::ConstantInt *AddrInt = Builder.getInt64(targetAddress);
  llvm::Value *FnPtr =
      Builder.CreateIntToPtr(AddrInt, llvm::PointerType::getUnqual(Ctx));

  std::vector<llvm::Value *> Args;
  for (llvm::Argument &Arg : TrampolineFn->args()) {
    Args.push_back(&Arg);
  }
  llvm::CallInst *Call = Builder.CreateCall(FnTy, FnPtr, Args);
  Call->setTailCallKind(llvm::CallInst::TCK_Tail);

  if (FnTy->getReturnType()->isVoidTy())
    Builder.CreateRetVoid();
  else
    Builder.CreateRet(Call);

  TargetFn->replaceAllUsesWith(TrampolineFn);
}

void call_self(llvm::Function &F) {
  // delete the original body of the function, we will create a self-recursive
  // call to hold all the arguments use references so that remill can lift the
  // function correctly
  F.deleteBody();

  llvm::LLVMContext &Ctx = F.getContext();
  llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(Ctx, "entry", &F);
  llvm::IRBuilder<> Builder(EntryBB);

  std::vector<llvm::Value *> Args;
  Args.reserve(F.arg_size());
  for (llvm::Argument &Arg : F.args())
    Args.push_back(&Arg);

  // create a self-recursive CallInst passing all arguments
  llvm::CallInst *RecursiveCall = Builder.CreateCall(&F, Args);

  if (F.getReturnType()->isVoidTy()) {
    Builder.CreateRetVoid();
  } else {
    Builder.CreateRet(RecursiveCall);
  }
}

void upgrade_svc_signature(llvm::Module &M, llvm::Function *OldFunc) {
  llvm::LLVMContext &Ctx = M.getContext();
  llvm::Type *PtrTy = llvm::PointerType::getUnqual(Ctx);
  llvm::Type *I64Ty = llvm::Type::getInt64Ty(Ctx);

  // build the new 4-argument FunctionType: (ptr, ptr, i64, ptr) -> returnType
  llvm::Type *RetTy = OldFunc->getReturnType();
  std::vector<llvm::Type *> NewParamTys = {PtrTy, PtrTy, I64Ty, PtrTy};
  llvm::FunctionType *NewFuncTy =
      llvm::FunctionType::get(RetTy, NewParamTys, false);

  // create the new Function declaration
  llvm::Function *NewFunc = llvm::Function::Create(
      NewFuncTy, OldFunc->getLinkage(), OldFunc->getAddressSpace(), "", &M);
  NewFunc->takeName(OldFunc);

  // copy attribute/calling convention metadata if applicable
  NewFunc->setCallingConv(OldFunc->getCallingConv());
  OldFunc->replaceAllUsesWith(NewFunc);

  call_self(*NewFunc);
}

void generate_naked_function(llvm::Function &Func, std::string_view asmbody) {
  llvm::LLVMContext &Ctx = Func.getContext();
  llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(Ctx, "entry", &Func);
  llvm::IRBuilder<> Builder(EntryBB);
  llvm::FunctionType *AsmFTy =
      llvm::FunctionType::get(Builder.getVoidTy(), false);
  llvm::InlineAsm *IA = llvm::InlineAsm::get(
      AsmFTy, asmbody,
      "",    // Constraints (empty string for basic top-level asm)
      true,  // hasSideEffects (prevents optimizer from removing/hoisting it)
      false, // isAlignStack
      llvm::InlineAsm::AD_ATT // Explicitly set AT&T dialect
  );
  Builder.CreateCall(IA);
  Builder.CreateUnreachable();
  Func.addFnAttr(llvm::Attribute::Naked);
}

void emit_opcode(std::string &asmbody, std::span<const uint8_t> opcode) {
  for (auto b : opcode)
    asmbody += std::format(".byte {:#x}\n", b);
}

} // namespace

std::map<uintptr_t, size_t> Lifter::dynhandlers;

std::set<HandlerDynamic> Lifter::aarch64;
std::set<HandlerDynamic> Lifter::x86;

Lifter::Lifter(const Binary *binptr, remill::Arch *ptr, llvm::Module *pre)
    : bin(binptr), arch(ptr), module(pre) {
  if (arch->arch_name == remill::kArchAArch64LittleEndian) {
    isel_handlers = &Handler::aarch64;
    handlers = &Lifter::aarch64;
  } else {
    isel_handlers = &Handler::x86;
    handlers = &Lifter::x86;
  }
}

Lifter::~Lifter() {
  // reset our handler container module to the arch as host's
  auto triple = module->getTargetTriple();
#if AETHER_ARCH_ARM64
  triple.setArch(llvm::Triple::aarch64);
#else
  triple.setArch(llvm::Triple::x86_64);
#endif
  module->setTargetTriple(triple);

  // optimize the lifted dynamic handlers
  remill::OptimizeModule(arch, module,
                         []() -> llvm::Function * { return nullptr; });

  // compile to in-memory object
  auto memobj = generate_object(*module);

  // parse and cache the newly generated handlers
  apply(memobj.get());

  // remove all the dynamically lifted handlers
  clear();
}

void Lifter::resetSemantic(llvm::Module &M) {
  llvm::Function *CallSupervisor = nullptr;
  for (llvm::Function &F : M) {
    if (F.isDeclaration())
      continue;

    if (F.getName().contains("CallSupervisor"))
      CallSupervisor = &F;
    else
      call_self(F);
  }

  // convert the SVC exception handler to the new signature with 4 arguments
  // to match the remill's svc instruction operand signature
  if (CallSupervisor)
    upgrade_svc_signature(M, CallSupervisor);

  for (auto &GV : M.globals()) {
    if (!GV.hasInitializer())
      continue;
    if (!GV.getName().starts_with("ISEL_"))
      continue;

    std::string TargetName{"."};
    TargetName += GV.getName().str();
    llvm::GlobalValue *ReferencedValue =
        llvm::dyn_cast<llvm::GlobalValue>(GV.getInitializer());
    // rename C++ name to the .ISEL name so that we can easily use binary search
    // later to find the real raw handler runtime address
    ReferencedValue->setName(TargetName);
  }
}

std::unique_ptr<llvm::MemoryBuffer>
Lifter::createObject(llvm::Module &M, std::span<const uint8_t> text) {
  llvm::LLVMContext &Ctx = M.getContext();
  llvm::ArrayRef<uint8_t> Bytes(text.data(), text.size());
  llvm::Constant *DataInit = llvm::ConstantDataArray::get(Ctx, Bytes);
  llvm::GlobalVariable *TextSecGV = new llvm::GlobalVariable(
      M, DataInit->getType(),
      /*isConstant=*/true, llvm::GlobalValue::ExternalLinkage, DataInit,
      "aethervm_snippet_entry");
  TextSecGV->setSection(".text");
  return generate_object(M);
}

void Lifter::transform(const llvm::MCInst &Inst,
                       std::span<const uint8_t> opcode) {
  HandlerDynamic placeholder;
  placeholder.entry = reinterpret_cast<uintptr_t>(&abort);
  std::memcpy(&placeholder.opc4, opcode.data(), opcode.size());
  if (handlers->find(placeholder) != handlers->end())
    return; // already lifted

  std::string name{dyn_prefix};
  for (auto b : opcode)
    name += std::format("{:02x}", b);
  auto [newit, result] = handlers->insert(placeholder);
  name_handlers.insert(
      std::make_pair(name, const_cast<HandlerDynamic *>(&*newit)));

  remill::Instruction inst;
  auto func = arch->DeclareLiftedFunction(name, module);
  std::ignore = arch->DecodeInstruction(
      0, {(char *)opcode.data(), (char *)opcode.data() + opcode.size()}, inst,
      arch->CreateInitialContext());
  if (!inst.IsValid()) {
    // Remill doesn't support this instruction, emit the raw instruction
    // directly if guest and host have the same architecture
#if AETHER_ARCH_ARM64
    if (bin->archType() == ARM64) {
      emitAArch64(*func, Inst, opcode);
      return;
    }
#else
    if (bin->archType() == X86_64) {
      emitX64(*func, Inst, opcode);
      return;
    }
#endif
  }

  auto intrinsics = arch->GetInstrinsicTable();
  arch->InitializeEmptyLiftedFunction(func);

  auto state_ptr = remill::NthArgument(func, remill::kStatePointerArgNum);
  auto body = llvm::BasicBlock::Create(module->getContext(), "", func);
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

  auto lifter = inst.GetLifter();
  auto lift_status = lifter->LiftIntoBlock(inst, body, state_ptr);
  if (remill::kLiftedInstruction != lift_status) {
    // Remill doesn't support this instruction, emit the raw instruction
    // directly if guest and host have the same architecture
#if AETHER_ARCH_ARM64
    if (bin->archType() == ARM64) {
      func->deleteBody();
      emitAArch64(*func, Inst, opcode);
      return;
    }
#else
    if (bin->archType() == X86_64) {
      func->deleteBody();
      emitX64(*func, Inst, opcode);
      return;
    }
#endif
  }
  auto tailcall = remill::kLiftedInstruction == lift_status
                      ? intrinsics->function_return
                      : intrinsics->error;
  remill::AddTerminatingTailCall(body, tailcall, *intrinsics);

  for (auto &I : llvm::instructions(*func)) {
    // CallBase covers both CallInst and InvokeInst
    if (auto *CB = llvm::dyn_cast<llvm::CallBase>(&I)) {
      llvm::Function *CalledFn = CB->getCalledFunction();
      if (CalledFn) {
        auto name = CalledFn->getName();
        if (name.starts_with(dyn_prefix))
          continue; // already replaced

        uint64_t target = reinterpret_cast<uint64_t>(&abort);
        auto find_target = [&target](const std::vector<Handler> *handlers,
                                     std::string_view name) {
          Handler key{hash_value(name), nullptr};
          auto base = handlers->data();
          auto found = binary_search(base, handlers->size(), key);
          if (is_exact(found, base, handlers->size(), key))
            target = reinterpret_cast<uint64_t>(found->impl);
        };
        if (name[0] == '.') {
          // isel handler, skip .ISEL_
          std::string_view iselname{name.data() + 6, name.size() - 6};
          find_target(isel_handlers, iselname);
        } else {
          // intrinsic handler
          find_target(&Handler::intrinsic, name);
        }
        create_trampoline(*module, CalledFn, target);
      }
    }
  }
}

void Lifter::emitAArch64(llvm::Function &Func, const llvm::MCInst &Inst,
                         std::span<const uint8_t> opcode) {
  // during the chained execution of the vm handlers:
  // x26 is "void *cpu"
  // x27 is 'const Instruction *insns'
  std::string asmbody;

  auto regused = aarch64::parse_regused(Inst);
  /*
  Registers        Role                          Saved By
  --------------------------------------------------------------
  x0  – x7        Arguments / Return Values        Caller
  x8              Indirect Result Location         Caller
  x9  – x15       Corruptible / Temporary          Caller
  x16 – x17       IP0 / IP1 (Linker/PLT Veneers)   Caller / Linker
  x18             Platform Register (e.g., TLS)    Platform-defined
  x19 – x28       Callee-saved GPRs                Callee
  x29             Frame Pointer (FP)               Callee (if framed)
  x30             Link Register (LR)               Callee (if non-leaf)
  sp              Stack Pointer                    Callee (aligned to 16 bytes)

  v0  – v7        Arguments / Return Values        Caller
  v8  – v15       Callee-saved (Lower 64-bits)     Callee (d8–d15 only)
  v16 – v31       Corruptible / Temporary          Caller
  */
  // save host context
  for (auto r : regused) {
    if (Register::X19 <= r && r < Register::X30)
      asmbody += std::format("str x{}, [sp, #-0x8]!\n",
                             19 + (int)r - (int)Register::X19);
    else if (Register::Q8 <= r && r < Register::Q15)
      asmbody += std::format("str d{}, [sp, #-0x8]!\n",
                             8 + (int)r - (int)Register::Q8);
  }

  // just use the original x26 or find a unused gpr as our cpu context
  auto regcpu = regused.find(Register::X26) == regused.end()
                    ? (int)Register::X26
                    : (int)Register::X0;
  while (regused.find((Register)regcpu) != regused.end())
    regcpu++;
  regcpu -= (int)Register::X0;
  if (regcpu != 26)
    asmbody += std::format("mov x{}, x26\n", regcpu);

  // load guest context
  for (auto r : regused) {
    if (Register::X0 <= r && r <= Register::X30)
      asmbody +=
          std::format("ldr x{}, [x{}, #{:#x}]\n", (int)r - (int)Register::X0,
                      regcpu, aarch64::offset_reg(r));
    else if (Register::Q0 <= r && r <= Register::Q30)
      asmbody +=
          std::format("ldr q{}, [x{}, #{:#x}]\n", (int)r - (int)Register::Q0,
                      regcpu, aarch64::offset_reg(r));
  }

  emit_opcode(asmbody, opcode);

  // save guest context
  for (auto r : regused) {
    if (Register::X0 <= r && r <= Register::X30)
      asmbody +=
          std::format("str x{}, [x{}, #{:#x}]\n", (int)r - (int)Register::X0,
                      regcpu, aarch64::offset_reg(r));
    else if (Register::Q0 <= r && r <= Register::Q30)
      asmbody +=
          std::format("str q{}, [x{}, #{:#x}]\n", (int)r - (int)Register::Q0,
                      regcpu, aarch64::offset_reg(r));
  }

  // load host context
  for (auto rit = regused.rbegin(), rend = regused.rend(); rit != rend; rit++) {
    auto r = *rit;
    if (Register::X19 <= r && r < Register::X29)
      asmbody += std::format("ldr x{}, [sp], #0x8\n",
                             19 + (int)r - (int)Register::X19);
    else if (Register::Q8 <= r && r < Register::Q15)
      asmbody +=
          std::format("ldr d{}, [sp], #0x8\n", 8 + (int)r - (int)Register::Q8);
  }

  // update pc
  asmbody += "mov x0, x26\n"          // argument state
             "ldr x1, [x0, #-0x10]\n" // load pcptr
             "ldr x2, [x1]\n"         // load pc
             "add x2, x2, #0x4\n"     // next pc
             "str x2, [x1]\n"         // set new pc
             "mov x1, x2\n";          // argument vmaddr

  // advance to the next instruction
  asmbody += "add x27, x27, #8\n"
             "mov x2, x27\n" // argument instruction
             "" extract_handler_x16 ""
             "br x16";

  generate_naked_function(Func, asmbody);
}

void Lifter::emitX64(llvm::Function &Func, const llvm::MCInst &Inst,
                     std::span<const uint8_t> opcode) {
  // during the chained execution of the vm handlers:
  // r12 is "void *cpu"
  // r13 is 'const Instruction *insns'
  std::string asmbody;

  auto regused = x86::parse_regused(Inst);
  /*
  Linux x86-64 (System V AMD64 ABI)

  Registers       Role                                          Saved By
  ------------------------------------------------------------------------------
  rax             1st Return Value / Indirect Result            Caller
  rdx             2nd Return Value / 3rd Argument               Caller
  rdi             1st Integer/Pointer Argument                  Caller
  rsi             2nd Integer/Pointer Argument                  Caller
  rcx             4th Integer/Pointer Argument                  Caller
  r8  – r9        5th and 6th Integer/Pointer Arguments         Caller
  r10             Static Chain Pointer / Temporary              Caller
  r11             Temporary / Scratch (PLT / Linker)            Caller
  r12 – r15       Callee-saved GPRs                             Callee
  rbx             Callee-saved GPR                              Callee
  rbp             Frame Pointer (FP) / Callee-saved             Callee
  rsp             Stack Pointer                                 Callee (aligned
  to 16 bytes)

  xmm0 – xmm1     FP/Vector Arguments & Return Values           Caller
  xmm2 – xmm7     FP/Vector Arguments                           Caller
  xmm8 – xmm15    FP/Vector Temporary / Scratch                 Caller


  Windows x64 (Microsoft x64 ABI)

  Registers       Role                                          Saved By
  ------------------------------------------------------------------------------
  rax             Return Value / Scratch                        Caller
  rcx             1st Integer/Pointer Argument                  Caller
  rdx             2nd Integer/Pointer Argument                  Caller
  r8  – r9        3rd and 4th Integer/Pointer Arguments         Caller
  r10 – r11       Temporary / Scratch                           Caller
  r12 – r15       Callee-saved GPRs                             Callee
  rbx, rsi, rdi   Callee-saved GPRs                             Callee
  rbp             Frame Pointer (FP) / Callee-saved (optional)  Callee
  rsp             Stack Pointer                                 Callee (aligned
  to 16 bytes)

  xmm0            1st FP/Vector Argument & Return Value         Caller
  xmm1 – xmm3     2nd, 3rd, and 4th FP/Vector Arguments         Caller
  xmm4 – xmm5     FP/Vector Temporary / Scratch                 Caller
  xmm6 – xmm15    Callee-saved (Lower 128-bits)                 Callee
  */
#if AETHER_OS_WINDOWS
  // Microsoft x64 ABI: rbx/rbp/rdi/rsi/r12-r15 are callee-saved GPRs;
  // xmm6-xmm15 are callee-saved too. r12/r13 reserved for cpu/insns.
  auto is_callee_saved_gpr = [](Register r) {
    return r == Register::RBX || r == Register::RBP || r == Register::RDI ||
           r == Register::RSI || r == Register::R14 || r == Register::R15;
  };
  auto is_callee_saved_xmm = [](Register r) {
    return r >= Register::XMM6 && r <= Register::XMM15;
  };
  // Volatile GPRs available to stand in for r12 if it's live in the guest
  // instruction; rdi/rsi/r14/r15 are NOT eligible here since Windows treats
  // them as callee-saved and this path never pushes/pops the stand-in.
  static constexpr Register kCpuCandidates[] = {
      Register::RAX, Register::RCX, Register::RDX, Register::R8,
      Register::R9,  Register::R10, Register::R11};
#else
  // System V AMD64 ABI: rbx/rbp/r12-r15 are callee-saved GPRs; no xmm
  // register is callee-saved. r12/r13 reserved for cpu/insns.
  auto is_callee_saved_gpr = [](Register r) {
    return r == Register::RBX || r == Register::RBP || r == Register::R14 ||
           r == Register::R15;
  };
  auto is_callee_saved_xmm = [](Register) { return false; };
  static constexpr Register kCpuCandidates[] = {
      Register::RAX, Register::RCX, Register::RDX, Register::RSI, Register::RDI,
      Register::R8,  Register::R9,  Register::R10, Register::R11};
#endif
  constexpr const char *kArgState = ARGREG_0;
  constexpr const char *kArgVmAddr = ARGREG_1;
  constexpr const char *kArgInsn = ARGREG_2;

  // save host context
  for (auto r : regused) {
    if (is_callee_saved_gpr(r))
      asmbody += std::format("push %{}\n", x86::gpr_name64(r));
  }
  for (auto r : regused) {
    if (is_callee_saved_xmm(r))
      asmbody += std::format("sub $$0x10, %rsp\n"
                             "movdqu %{}, (%rsp)\n",
                             x86::xmm_name(r));
  }

  // just use the original r12 or find an unused, ABI-volatile gpr as our
  // cpu context register (candidates differ per platform above, since a
  // callee-saved register standing in here would need its own save/restore
  // that this loop doesn't do)
  Register regcpu = Register::R12;
  if (regused.find(Register::R12) != regused.end()) {
    for (auto cand : kCpuCandidates) {
      if (regused.find(cand) == regused.end()) {
        regcpu = cand;
        break;
      }
    }
  }
  if (regcpu != Register::R12)
    asmbody += std::format("mov %r12, %{}\n", x86::gpr_name64(regcpu));

  // load guest context
  for (auto r : regused) {
    if (x86::is_gpr64(r))
      asmbody += std::format("mov {:#x}(%{}), %{}\n", x86::offset_reg(r),
                             x86::gpr_name64(regcpu), x86::gpr_name64(r));
    else if (r >= Register::XMM0 && r <= Register::XMM15)
      asmbody += std::format("movdqu {:#x}(%{}), %{}\n", x86::offset_reg(r),
                             x86::gpr_name64(regcpu), x86::xmm_name(r));
  }

  emit_opcode(asmbody, opcode);

  // save guest context
  for (auto r : regused) {
    if (x86::is_gpr64(r))
      asmbody += std::format("mov %{}, {:#x}(%{})\n", x86::gpr_name64(r),
                             x86::offset_reg(r), x86::gpr_name64(regcpu));
    else if (r >= Register::XMM0 && r <= Register::XMM15)
      asmbody += std::format("movdqu %{}, {:#x}(%{})\n", x86::xmm_name(r),
                             x86::offset_reg(r), x86::gpr_name64(regcpu));
  }

  // load host context (reverse of the two save loops above, XMM block first
  // since it was pushed last)
  for (auto rit = regused.rbegin(), rend = regused.rend(); rit != rend; rit++) {
    if (is_callee_saved_xmm(*rit))
      asmbody += std::format("movdqu (%rsp), %{}\n"
                             "add $$0x10, %rsp\n",
                             x86::xmm_name(*rit));
  }
  for (auto rit = regused.rbegin(), rend = regused.rend(); rit != rend; rit++) {
    if (is_callee_saved_gpr(*rit))
      asmbody += std::format("pop %{}\n", x86::gpr_name64(*rit));
  }

  // update pc
  asmbody += "mov %r12, %rax\n"        // stash cpu ptr; rax is volatile
                                       // on both ABIs and isn't an arg reg
                                       // on either, so safe as scratch
             "mov -0x10(%rax), %rcx\n" // load pcptr into scratch (rcx: not
                                       // yet holding an arg value at this
                                       // point on either ABI)
             "mov (%rcx), %rdx\n";     // load pc into scratch
  asmbody += std::format("add $${:#x}, %rdx\n", opcode.size()); // next pc
  asmbody += "mov %rdx, (%rcx)\n";                              // set new pc
  asmbody += std::format("mov %rax, %{}\n", kArgState);  // argument: state
  asmbody += std::format("mov %rdx, %{}\n", kArgVmAddr); // argument: vmaddr

  // advance to the next instruction
  asmbody += "add $$8, %r13\n";
  asmbody += std::format("mov %r13, %{}\n", kArgInsn); // argument: instruction
  asmbody += extract_handler_r10_llvmir;
  asmbody += "jmp *%r10\n";

  generate_naked_function(Func, asmbody);
}

void Lifter::apply(llvm::MemoryBuffer *mbuf) {
  llvm::MemoryBufferRef bufferRef(*mbuf);
  auto expObject = llvm::object::ObjectFile::createObjectFile(bufferRef);
  if (!expObject) {
    log_print(Runtime, "Fatal error, failed to create llvm object.");
    return;
  }
  uint64_t textaddr = 0;
  llvm::StringRef textbuff;
  std::map<addr_t, addr_t> relocrefs;
  std::set<addr_t> jumps;
  for (auto sect : expObject.get()->sections()) {
    auto expName = sect.getName();
    if (expName && expName.get() == ".text") {
      for (auto &r : sect.relocations()) {
        auto sym = r.getSymbol();
        auto toExp = sym->getValue();
        if (!toExp)
          continue;
        auto from = r.getOffset();
        auto to = toExp.get();
        auto name = sym->getName();
        // tail call of __remill intrinsic is a jump
        // normal call for other handlers
        if (name && name->contains("__remill"))
          jumps.insert(from);
        if (name->starts_with(dyn_prefix))
          relocrefs.insert(std::make_pair(from, to));
      }
      auto expBuff = sect.getContents();
      if (expBuff) {
        textbuff = expBuff.get();
        textaddr = sect.getAddress();
        break;
      }
    }
  }
  if (!textbuff.size()) {
    log_print(Runtime, "Fatal error, failed to parse .text section.");
    return;
  }

  // put the dynamic handlers into executable page
  auto pagesize = align_up(textbuff.size(), page_size());
  auto pagestart = page_alloc(pagesize);
  auto pagebuff = reinterpret_cast<void *>(pagestart);
  // rw
  if (!page_commit(pagebuff, pagesize, true, true, false)) {
    log_print(
        Runtime,
        "Fatal error, failed to allocate in-memory object buffer, size {}.",
        pagesize);
    return;
  }
  // copy executable code
  std::memcpy(pagebuff, textbuff.data(), textbuff.size());
  // fix relocation
#if AETHER_ARCH_ARM64
  for (auto [from, to] : relocrefs) {
    auto opcptr = reinterpret_cast<uint32_t *>(pagestart + from);
    bool call = jumps.find(from) == jumps.end();
    opcptr[0] = bin->genBranchOpcode(from, to, call);
  }
#else
  for (auto [fr, to] : relocrefs) {
    // go back 1 byte to patch the call/jump opcode
    auto from = fr - 1;
    auto opcptr = reinterpret_cast<char *>(pagestart + from);
    bin->patchCallOffset(opcptr, from, to);
  }
#endif
  // rx
  if (!page_commit(pagebuff, pagesize, true, false, true)) {
    log_print(Runtime,
              "Fatal error, failed to set in-memory object buffer {} "
              "readonly-executable.",
              (void *)pagebuff);
    return;
  }
  dynhandlers.insert(std::make_pair(pagestart, pagesize));

  using SymbolRef = llvm::object::SymbolRef;
  for (auto sym : expObject.get()->symbols()) {
    auto expType = sym.getType();
    if (!expType)
      continue;
    if (expType.get() != SymbolRef::ST_Function)
      continue;
    auto expFlags = sym.getFlags();
    if (!expFlags)
      continue;
    auto flags = expFlags.get();
    if ((flags & SymbolRef::SF_Undefined) || (flags & SymbolRef::SF_Common) ||
        (flags & SymbolRef::SF_Indirect) ||
        (flags & SymbolRef::SF_FormatSpecific)) {
      continue;
    }
    auto expAddr = sym.getAddress();
    auto expName = sym.getName();
    if (!expAddr || !expName)
      continue;
    auto name = expName.get().str();
    if (!name.starts_with(dyn_prefix))
      continue;
    auto addr = expAddr.get();
    auto opcode = name.data() + dyn_prefix.size();
    // find and reset the entry of newly created handler
    if (isxdigit(opcode[0])) {
      auto found = name_handlers.find(name);
      if (found == name_handlers.end()) {
        // should never happen
        log_print(Runtime, "Failed to find {}.", name);
        continue;
      }
      // reset entry
      found->second->entry = pagestart + addr - textaddr;
    }
  }
}

void Lifter::clear() {
  std::vector<llvm::Function *> newdyns;

  for (llvm::Function &F : *module) {
    auto name = F.getName();
    if (name.starts_with(dyn_prefix) && isxdigit(name[dyn_prefix.size()]))
      newdyns.push_back(&F);
  }

  for (llvm::Function *F : newdyns) {
    F->replaceAllUsesWith(llvm::UndefValue::get(F->getType()));
    F->eraseFromParent();
  }
}

} // namespace aether

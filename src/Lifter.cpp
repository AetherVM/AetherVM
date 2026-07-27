// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Lifter.h>
#include <Platform.h>
#include <Utils.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include <remill/BC/ABI.h>
#include <remill/BC/Optimizer.h>
#include <remill/BC/Util.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace aether {

namespace {

llvm::Function *isel_function(llvm::Module &module, std::string_view function) {
  std::stringstream ss;
  ss << "ISEL_" << function;
  auto isel_name = ss.str();

  auto isel = remill::FindGlobaVariable(&module, isel_name);
  auto sem = isel->getInitializer()->stripPointerCasts();
  return llvm::dyn_cast_or_null<llvm::Function>(sem);
}

llvm::Function *copy_decl(const llvm::Function &srcFunc,
                          llvm::Module &dstModule) {
  // Check if declaration already exists
  if (auto existing = dstModule.getFunction(srcFunc.getName()))
    return existing;

  llvm::Function *dstFunc = llvm::Function::Create(
      srcFunc.getFunctionType(), srcFunc.getLinkage(),
      srcFunc.getAddressSpace(), srcFunc.getName(), &dstModule);

  // Copy attributes & calling convention
  dstFunc->setCallingConv(srcFunc.getCallingConv());
  dstFunc->setVisibility(srcFunc.getVisibility());
  dstFunc->setAttributes(srcFunc.getAttributes());

  return dstFunc;
}

std::unique_ptr<llvm::MemoryBuffer> generate_object(llvm::Module &module) {
  auto triple = module.getTargetTriple();
  std::string errorStr;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(triple, errorStr);

  llvm::TargetOptions opt;
  auto targetMachine =
      std::unique_ptr<llvm::TargetMachine>(target->createTargetMachine(
          triple, "generic", "+all", opt, llvm::Reloc::PIC_, std::nullopt,
          llvm::CodeGenOptLevel::Default));

  llvm::SmallVector<char, 0> buffer;
  llvm::raw_svector_ostream os(buffer);

  llvm::legacy::PassManager pm;
  targetMachine->addPassesToEmitFile(pm, os, nullptr,
                                     llvm::CodeGenFileType::ObjectFile);
  pm.run(module);

  return llvm::MemoryBuffer::getMemBufferCopy(
      llvm::StringRef(buffer.data(), buffer.size()), "aethervm_object");
}

} // namespace

std::vector<uintptr_t> Lifter::stubs;
size_t Lifter::stub_pageoff = 0;

std::map<uintptr_t, size_t> Lifter::objects;

std::set<HandlerDynamic> Lifter::arch64;
std::set<HandlerDynamic> Lifter::x86;

Lifter::Lifter(remill::Arch *ptr, const llvm::Module *pre)
    : arch(ptr), prebuilt(pre), module("aethervm", pre->getContext()) {
  arch->PrepareModule(&module);

  if (!stubs.size())
    stubs.push_back(page_alloc(page_size()));
}

Lifter::~Lifter() {
  // optimize the lifted dynamic handlers
  remill::OptimizeModule(arch, &module,
                         []() -> llvm::Function * { return nullptr; });

  // compile to in-memory object
  auto memobj = generate_object(module);
#if AETHER_DEBUG
  auto savepath = fs::temp_directory_path() / "aethervm.obj";
  std::ofstream outf(savepath, std::ios::binary);
  outf.write(memobj->getBufferStart(), memobj->getBufferSize());
  outf.close();
  log_print(Develop, "Saved in-memory object {}.", savepath.string());
#endif

  // put the dynamic handlers into executable page
  auto pagesize = align_up(memobj->getBufferSize(), page_size());
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
  std::memcpy(pagebuff, memobj->getBufferStart(), memobj->getBufferSize());

  // relocate the raw handler references and cache the newly generated handlers
  apply(pagestart, pagesize);
}

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

  auto lifter = inst.GetLifter();
  auto isel_func = isel_function(module, inst.function);
  copy_decl(*isel_func, module);

  auto lift_status = lifter->LiftIntoBlock(inst, body, state_ptr);
  auto tailcall = remill::kLiftedInstruction == lift_status
                      ? intrinsics->function_return
                      : intrinsics->error;
  remill::AddTerminatingTailCall(body, tailcall, *intrinsics);
}

void Lifter::apply(uintptr_t pagestart, size_t pagesize) {
  auto pagebuff = reinterpret_cast<char *>(pagestart);
  llvm::StringRef bufferData(pagebuff, pagesize);
  llvm::MemoryBufferRef bufferRef(bufferData, "aethervm-object");
  auto expObject = llvm::object::ObjectFile::createObjectFile(bufferRef);
  if (!expObject) {
    log_print(Runtime, "Fatal error, failed to create llvm object.");
    return;
  }
  for (auto sect : expObject.get()->sections()) {
    // parse relocations
  }

  // rx
  if (!page_commit(pagebuff, pagesize, true, false, true)) {
    log_print(Runtime,
              "Fatal error, failed to set in-memory object buffer {} "
              "readonly-executable.",
              (void *)pagebuff);
    return;
  }
  objects.insert(std::make_pair(pagestart, pagesize));
}

} // namespace aether

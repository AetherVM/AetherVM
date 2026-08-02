// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Lifter.h"
#include <Platform.h>
#include <Utils.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/CodeGen.h>
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

#if AETHER_DEBUG
  auto savepath = fs::temp_directory_path() / "aethervm.obj";
  std::ofstream outf(savepath, std::ios::binary);
  outf.write(buffer.data(), buffer.size());
  outf.close();
  log_print(Develop, "Saved in-memory object {}.", savepath.string());
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
  for (llvm::Function &F : M) {
    if (!F.isDeclaration())
      F.deleteBody();
  }

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

void Lifter::transform(std::span<const uint8_t> opcode) {
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

  auto intrinsics = arch->GetInstrinsicTable();
  auto func = arch->DeclareLiftedFunction(name, module);
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

  remill::Instruction inst;
  std::ignore = arch->DecodeInstruction(
      0, {(char *)opcode.data(), (char *)opcode.data() + opcode.size()}, inst,
      arch->CreateInitialContext());

  auto lifter = inst.GetLifter();
  auto lift_status = lifter->LiftIntoBlock(inst, body, state_ptr);
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
  for (auto [from, to] : relocrefs) {
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
    auto addr = expAddr.get();
    auto name = expName.get().str();
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
    if (F.getName().starts_with(dyn_prefix))
      newdyns.push_back(&F);
  }

  for (llvm::Function *F : newdyns) {
    F->replaceAllUsesWith(llvm::UndefValue::get(F->getType()));
    F->eraseFromParent();
  }
}

} // namespace aether

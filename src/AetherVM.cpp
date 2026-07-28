// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

// must include before AArch64.h and X86.h to make remill definition available
// for both of them
#include <remill/Arch/Runtime/State.h>
#include <remill/Arch/Runtime/Types.h>
#include <remill/BC/Util.h>
#include <remill/OS/OS.h>

#include <AArch64.h>
#include <AetherVM.h>
#include <Disassembler.h>
#include <Event.h>
#include <Handler.h>
#include <Lifter.h>
#include <Memory.h>
#include <Utils.h>
#include <X86.h>

#include <llvm/MC/MCInst.h>

#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

namespace aether {

struct BinaryEngineImpl {
  ArchType arch;
  GuestMemory guestMemory;

  std::mutex mutex;
  std::vector<EventCallback> eventCallbacks;

  llvm::LLVMContext llvmContext;
  remill::Arch::ArchPtr remillArch;
  std::unique_ptr<llvm::Module> remillSemantic;

  BinaryEngineImpl(ArchType arch, FileType os);
  ~BinaryEngineImpl();

  bool startVM(addr_t entry);
};

struct CPUState {
  // 1MB stack size for each thread
  static constexpr size_t stackSize = 1 * 1024 * 1024;

  BinaryEngineImpl *runtime = nullptr;
  char *stack = nullptr;
  union {
    aarch64::State aarch64;
    x86::State x86;
  };

  CPUState() {}
  ~CPUState() {}

  bool initContext(addr_t entry);

  // should be explicitly called after child thread has exited
  void freeContext();

  const RegisterValue *getRegisterAArch64(Register reg);
  bool setRegisterAArch64(Register reg, RegisterValue val);

  const RegisterValue *getRegisterX86(Register reg);
  bool setRegisterX86(Register reg, RegisterValue val);
};

bool CPUState::initContext(addr_t entry) {
  // init stack buffer if necessary
  if (!stack) {
    if (!(stack = new char[stackSize]))
      return false;
  }

  auto setRegister = [this](Register reg, RegisterValue val) {
    runtime->arch == ARM64 ? setRegisterAArch64(reg, val)
                           : setRegisterX86(reg, val);
  };
  RegisterValue pc{.b8 = entry};
  RegisterValue sp{.ptr = stack + stackSize};
  // reset SP and PC
  setRegister(Register::PC, pc);
  setRegister(Register::SP, sp);
  return true;
}

void CPUState::freeContext() {
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

// execution state for each thread
static thread_local CPUState CPU;

// shortcuts for engine implementation
#define lock_on() std::lock_guard<std::mutex> _lock_(mutex)

BinaryEngineImpl::BinaryEngineImpl(ArchType type, FileType os) : arch(type) {
  using enum remill::ArchName;
  using enum remill::OSName;
  // use windows coff as our in-memory object anyway
  remill::OSName os_name = kOSWindows;
  remill::ArchName arch_name;
  // lazily load handlers, only support AArch64 and X86_64
  if (arch == ARM64) {
    Handler::loadAArch64();
    arch_name = kArchAArch64LittleEndian;
  } else {
    Handler::loadX86();
    arch_name = kArchAMD64_AVX512;
  }
  remillArch = remill::Arch::Get(llvmContext, os_name, arch_name);
  remillSemantic = remill::LoadArchSemantics(
      remillArch.get(), {fs::path(self_path()).parent_path() / "bitcode"});
  CPU.runtime = this;
  // remove all the handlers' definition as we have built them into AetherVM
  // itself
  for (llvm::Function &F : *remillSemantic) {
    if (!F.isDeclaration())
      F.deleteBody();
  }
}

BinaryEngineImpl::~BinaryEngineImpl() { CPU.freeContext(); }

bool BinaryEngineImpl::startVM(addr_t entry) {
  if (!CPU.initContext(entry))
    return false;
  return false;
}

// shortcuts for engine implementation stub
#define engine ((BinaryEngineImpl *)m_impl)
#define callbacks (engine->eventCallbacks)
#define memory (engine->guestMemory)

BinaryEngine::BinaryEngine(const Machine *mach) : m_machine(mach) {
#if AETHER_OS_MACOS
  auto os = MachO;
#elif AETHER_OS_LINUX
  auto os = ELF;
#elif AETHER_OS_WINDOWS
  auto os = PE;
#else
#error AetherVM only supports macOS, Linux, and Windows
#endif
  m_impl = new BinaryEngineImpl(mach->archType(), os);
}

BinaryEngine::BinaryEngine(const Binary *bin) : m_binary(bin) {
  m_impl = new BinaryEngineImpl(bin->archType(), bin->fileType());
  // use the image base from bin so that we can directly map section memory
  memory.baseGuest = bin->imageBase();
  // calculate the total page size of all the sections
  size_t size = 0;
  for (auto &[addr, sect] : bin->sections())
    size += sect.size;
  size = align_up(size, page_size());

  // map all the sections
  memory.commit(bin->imageBase(), size, true, true);
  for (auto &[addr, sect] : bin->sections()) {
    if (!sect.size)
      continue;
    auto sectbuff = reinterpret_cast<const uint8_t *>(bin->addrBuff(sect.addr));
    writeMemory(sect.addr, {sectbuff, sect.size});
    if (sect.type == TEXT)
      liftOpcodes({sectbuff, sect.size});
  }
}

BinaryEngine::~BinaryEngine() { delete engine; }

bool BinaryEngine::execute(std::span<const uint8_t> raw) {
  auto vmaddr = mapMemory(raw.size());
  if (!vmaddr)
    return false;

  writeMemory(vmaddr, raw);
  liftOpcodes(raw);
  return execute(vmaddr);
}

bool BinaryEngine::execute(addr_t target) {
  if (!memory.valid(target, 1))
    return false;

  return engine->startVM(target);
}

bool BinaryEngine::runMain() { return false; }

const void *BinaryEngine::makeExecutable(std::span<const uint8_t> raw) {
  return nullptr;
}

const RegisterValue *BinaryEngine::getRegister(Register reg) {
  return engine->arch == ARM64 ? CPU.getRegisterAArch64(reg)
                               : CPU.getRegisterX86(reg);
}

bool BinaryEngine::setRegister(Register reg, RegisterValue val) {
  // automatically set and managed by each thread cpu state
  if (reg == Register::SP)
    return false;
  return engine->arch == ARM64 ? CPU.setRegisterAArch64(reg, val)
                               : CPU.setRegisterX86(reg, val);
}

addr_t BinaryEngine::mapMemory(size_t size) {
  auto vmaddr = memory.guestAvailable();
  size = align_up(size, page_size());
  return memory.commit(vmaddr, size, true, true) ? vmaddr : 0;
}

std::vector<uint8_t> BinaryEngine::readMemory(addr_t addr, size_t size) {
  std::vector<uint8_t> buff;
  if (memory.valid(addr, size)) {
    buff.resize(size);
    std::memcpy(buff.data(), reinterpret_cast<void *>(memory.host(addr)), size);
  }
  return buff;
}

uint64_t BinaryEngine::readUInt64(addr_t addr) {
  uint64_t result = 0;
  if (memory.valid(addr, sizeof(result)))
    std::memcpy(&result, reinterpret_cast<void *>(memory.host(addr)),
                sizeof(result));
  return result;
}

bool BinaryEngine::writeMemory(addr_t addr, std::span<const uint8_t> buff) {
  if (!memory.valid(addr, buff.size()))
    return false;
  std::memcpy(reinterpret_cast<void *>(memory.host(addr)), buff.data(),
              buff.size());
  return true;
}

int BinaryEngine::registerCallback(EventCallback callback) {
  callbacks.push_back(callback);
  return (int)callbacks.size();
}

void BinaryEngine::liftOpcodes(std::span<const uint8_t> opcodes) {
  llvm::MCInst inst;
  Lifter lifter{const_cast<remill::Arch *>(engine->remillArch.get()),
                engine->remillSemantic.get()};
  auto arch = m_machine ? m_machine->archType() : m_binary->archType();
  if (arch == ARM64) {
    // arm64 has fixed 4 bytes instruction set
    constexpr size_t oplen = 4;
    Disassembler diser{Binary::arch(ARM64)};
    for (auto ptr = opcodes.data(), end = ptr + opcodes.size(); ptr < end;
         ptr += oplen) {
      if (diser.disassemble(ptr, 16, inst) == oplen)
        lifter.transform({ptr, oplen});
    }
  } else {
    Disassembler diser{Binary::arch(X86_64)};
    MachineX86 mx86;
    // iterate each x86 instruction
    for (auto ptr = opcodes.data(), end = ptr + opcodes.size(); ptr < end;) {
      size_t oplen = diser.disassemble(ptr, 16, inst);
      if (!oplen) {
        oplen = mx86.defaultSize();
        ptr += oplen;
        continue;
      }
      lifter.transform({ptr, oplen});
    }
  }
}

} // namespace aether

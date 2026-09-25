// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"

#include "aes.c"
#include "elf_hash.c"

namespace {

constexpr uint32_t arm64_ret = 0xD65F03C0;
constexpr uint8_t x64_ret = 0xC3;

void aarch64_opcodes() {
  aether::MachineARM64 marm64;
  aether::BinaryEngine engine{&marm64};

  auto insn = "nop";
  auto opcode = assemble(&marm64, insn);
  if (!engine.emulate(opcode))
    std::println("Failed to execute: {}.", insn);

  insn = "mov x0, #1";
  opcode = assemble(&marm64, insn);
  if (engine.emulate(opcode))
    std::println("X0 = {}", engine.getRegister(aether::Register::X0)->u8);
  else
    std::println("Failed to execute: {}.", insn);

  engine.emulate(assemble(&marm64, "fmov d0, #1.0"));
#if AETHER_ARCH_ARM64
  insn = "dup v0.2d, v0.d[0]";
#else
  insn = "fmov d1, d0"
#endif
  engine.emulate(assemble(&marm64, insn));
#if AETHER_ARCH_ARM64
  auto q0 = reinterpret_cast<const aether::RegisterValueSIMD *>(
      engine.getRegister(aether::Register::Q0));
  std::println("Q0.D[0] = {:.1f}, Q0.D[1] = {:.1f}", q0->low.d, q0->high.d);
#else
  auto q0 = engine.getRegister(aether::Register::Q0);
  auto q1 = engine.getRegister(aether::Register::Q1);
  std::println("Q0.D[0] = {:.1f}, Q1.D[0] = {:.1f}", q0->d, q1->d);
#endif
}

void x64_opcodes() {
  aether::MachineX86 mx64;
  aether::BinaryEngine engine{&mx64};
  engine.prefetch();

  auto insn = "nop";
  auto opcode = assemble(&mx64, insn);
  if (!engine.emulate(opcode))
    std::println("Failed to execute: {}.", insn);

  insn = "mov $1, %rax";
  opcode = assemble(&mx64, insn);
  if (engine.emulate(opcode))
    std::println("RAX = {}", engine.getRegister(aether::Register::RAX)->u8);
  else
    std::println("Failed to execute: {}.", insn);

  engine.emulate(assemble(&mx64, "cvtsi2sd %eax, %xmm0"));
  engine.emulate(assemble(&mx64, "unpcklpd %xmm0, %xmm0"));
  auto xmm0 = reinterpret_cast<const aether::RegisterValueSIMD *>(
      engine.getRegister(aether::Register::XMM0));
  std::println("XMM0.D[0] = {:.1f}, XMM0.D[1] = {:.1f}", xmm0->low.d,
               xmm0->high.d);
}

void log_hash_elf(std::string_view arch, uint32_t hash) {
  std::cout << std::setfill(' ') << std::setw(8) << arch << ": elf_hash = 0x"
            << std::hex << hash << std::endl;
}

void execute_hash_elf(std::string_view script, std::string_view arch,
                      std::string_view name, bool debug) {
  // emulate the call to 'elf_hash(name.c_str())' in the obj file
  auto dir = fs::absolute(script).parent_path();
  auto obj = (dir / std::format("elf_hash.{}.obj", arch)).string();
  auto bin = aether::New(obj.c_str());
  auto &func = bin->functions().begin()->second;
  bool arm64 = bin->archType() == aether::ARM64;

  // the object file is compiled with MSVC ABI: clang -c elf_hash.c -target
  // x86_64-msvc-windows -o elf_hash.x86_64.obj -O2
  // so the first argument is passed in RCX register
  auto argreg = arm64 ? aether::Register::X0 : aether::Register::RCX;
  auto retreg = arm64 ? aether::Register::X0 : aether::Register::RAX;

  std::span<const uint8_t> insn_ret{arm64 ? (uint8_t *)&arm64_ret : &x64_ret,
                                    arm64 ? sizeof(arm64_ret)
                                          : sizeof(x64_ret)};
  aether::MachineARM64 marm64;
  aether::MachineX86 mx64;
  auto mach = arm64 ? (aether::Machine *)&marm64 : (aether::Machine *)&mx64;

  aether::EventConfig eventcfg;
  eventcfg.debug = debug;

  aether::BinaryEngine engine{mach, eventcfg};
  // initialize the first argument
  engine.setRegister(argreg, {.str = name.data()});

  // call the elf_hash function using opcode emulation
  auto opcstart = (const uint8_t *)bin->addrBuff(func.start);
  auto opcend = opcstart + func.end - func.start;
  // set the current pc=opcstart
  engine.prefetch({opcstart, opcend});
  engine.setRegister(aether::Register::PC, {.u8 = (uint64_t)opcstart});
  while (true) {
    auto opc = engine.getRegister(aether::Register::PC, false)->u1p;
    if (std::memcmp(insn_ret.data(), opc, insn_ret.size()) == 0)
      break;
    engine.emulate({opc, 16});
  }
  log_hash_elf(arch, engine.getRegister(retreg)->u4);
  aether::Delete(bin);
}

void log_result(std::string_view arch, std::string_view result) {
  std::cout << std::setfill(' ') << std::setw(8) << arch
            << ": result = " << result << std::endl;
}

void execute_endec(std::string_view script, std::string_view arch,
                   std::string_view text, bool debug) {
  // emulate the call to 'test_main(text.data(), text.size(), result)' in the
  // library file
  auto dir = fs::absolute(script).parent_path();
  auto lib = (dir / std::format("aes.{}", arch)).string();
  auto bin = aether::New(lib.c_str());
  bool arm64 = bin->archType() == aether::ARM64;
  aether::Function *func = nullptr;
  for (auto &[addr, fn] : bin->functions()) {
    if (fn.name.contains("test_main")) {
      func = &fn;
      break;
    }
  }

  aether::MachineARM64 marm64;
  aether::MachineX86 mx64;
  auto mach = arm64 ? (aether::Machine *)&marm64 : (aether::Machine *)&mx64;

  // the library file is compiled with Darwin ABI: clang aes.c -target
  // x86_64-apple-macosx -o aes.x86_64 -O2 -shared -fno-stack-protector,
  // so the arguments are passed in RDI/RSI/RDX register
  aether::Register arm64_argregs[] = {
      aether::Register::X0, aether::Register::X1, aether::Register::X2};
  aether::Register x86_64_argregs[] = {
      aether::Register::RDI, aether::Register::RSI, aether::Register::RDX};
  auto argregs =
      bin->archType() == aether::ARM64 ? &arm64_argregs[0] : &x86_64_argregs[0];
  auto retreg = bin->archType() == aether::ARM64 ? aether::Register::X0
                                                 : aether::Register::RAX;

  aether::EventConfig eventcfg;
  eventcfg.debug = debug;
  aether::BinaryEngine engine{mach, eventcfg};
  engine.setOpcodeBinary(bin);
  // initialize the arguments
  char result[64];
  engine.setRegister(argregs[0], {.str = text.data()});
  engine.setRegister(argregs[1], {.u8 = text.size()});
  engine.setRegister(argregs[2], {.str = &result[0]});

  auto ret_opcsz = arm64 ? sizeof(arm64_ret) : sizeof(x64_ret);
  auto sect = bin->addrSect(func->start);
  auto sectstart = (const uint8_t *)bin->addrBuff(sect->addr);
  auto sectend = sectstart + sect->size;
  // prefect all the opcode in text section
  engine.prefetch({sectstart, sectend});

  // call the test_main function
  auto fnstart = (const uint8_t *)bin->addrBuff(func->start);
  engine.setRegister(aether::Register::PC, {.u8 = (uint64_t)fnstart});
  while (true) {
    auto pc = engine.getRegister(aether::Register::PC);
    if (pc->u8 + ret_opcsz == func->end)
      break;
    engine.emulate({pc->u1p, 16});
  }
  log_result(arch, engine.getRegister(retreg)->str);
  aether::Delete(bin);
}

} // namespace

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  std::string_view symbol = "AetherVM";
  bool debug = argc > 1 && strcmp(argv[1], "debug") == 0;

  /*
  raw opcodes
  */
  if (1) {
    aarch64_opcodes();
    x64_opcodes();
  }

  /*
  hash_elf algorithm
  */
  if (1) {
    log_hash_elf("host", elf_hash(reinterpret_cast<const unsigned char *>(
                             symbol.data())));

    for (auto arch : {"arm64", "x86_64"})
      execute_hash_elf(argv[0], arch, symbol, debug);
  }

  /*
  aes algorithm
  */
  if (1) {
    char result[64];
    log_result("host", test_main(symbol.data(), symbol.size(), result));

    // beacuse the aes algorithm binary file was compiled with advanced NEON and
    // SSE/AVX instructions which some of them are not supported by Remill, so
    // we only emulate them in the same host architecture in order that the
    // native execution can take care of them.
    auto arch =
#if AETHER_ARCH_ARM64
        "arm64"
#else
        "x86_64"
#endif
        ;
    execute_endec(argv[0], arch, symbol, debug);
  }
  return 0;
}

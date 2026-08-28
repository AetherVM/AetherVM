// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"

#include "aes.c"

namespace {

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
  aether::Function *func = nullptr;
  for (auto &[addr, fn] : bin->functions()) {
    if (fn.name.contains("test_main")) {
      func = &fn;
      break;
    }
  }

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
  aether::BinaryEngine engine{bin, eventcfg};
  // initialize the arguments
  char result[64];
  engine.setRegister(argregs[0], {.str = text.data()});
  engine.setRegister(argregs[1], {.u8 = text.size()});
  engine.setRegister(argregs[2], {.str = &result[0]});
  // call elf_hash function
  if (engine.execute(func->start))
    log_result(arch, engine.getRegister(retreg)->str);
  else
    std::cerr << arch << ": execute endec failed" << std::endl;

  aether::Delete(bin);
}

} // namespace

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  std::string_view text = "AetherVM";
  char result[64];

  log_result("host", test_main(text.data(), text.size(), result));

  bool debug = argc > 1 && strcmp(argv[1], "debug") == 0;
  // beacuse the aes algorithm binary file was compiled with advanced NEON and
  // SSE/AVX instructions which some of them are not supported by Remill, so we
  // only emulate them in the same host architecture in order that the native
  // execution can take care of them.
  auto arch =
#if AETHER_ARCH_ARM64
      "arm64"
#else
      "x86_64"
#endif
      ;
  execute_endec(argv[0], arch, text, debug);
  return 0;
}

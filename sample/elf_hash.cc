// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "elf_hash.c"
#include "common.h"

namespace {

void log_hash_elf(std::string_view arch, uint32_t hash) {
  std::cout << std::setfill(' ') << std::setw(8) << arch << ": elf_hash = 0x"
            << std::hex << hash << std::endl;
}

void execute_hash_elf(std::string_view script, std::string_view arch,
                      std::string_view name) {
  // emulate the call to 'elf_hash(name.c_str())' in the obj file
  auto dir = fs::absolute(script).parent_path();
  auto obj = (dir / std::format("elf_hash.{}.obj", arch)).string();
  auto bin = aether::New(obj.c_str());
  auto &func = bin->functions().begin()->second;

  // the object file is compiled with MSVC ABI: clang -c elf_hash.c -target
  // x86_64-msvc-windows -o elf_hash.x86_64.obj -O2
  // so the first argument is passed in RCX register
  auto argreg = bin->archType() == aether::ARM64 ? aether::Register::X0
                                                 : aether::Register::RCX;
  auto retreg = bin->archType() == aether::ARM64 ? aether::Register::X0
                                                 : aether::Register::RAX;

  aether::BinaryEngine engine{bin};
  // initialize the first argument
  engine.setRegister(argreg, {.str = name.data()});
  // call elf_hash function
  if (engine.execute(func.start))
    log_hash_elf(arch, engine.getRegister(retreg)->u4);
  else
    std::cerr << arch << ": execute elf_hash failed" << std::endl;

  aether::Delete(bin);
}

} // namespace

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  std::string_view symbol = "AetherVM";
  log_hash_elf(
      "host", elf_hash(reinterpret_cast<const unsigned char *>(symbol.data())));

  for (auto arch : {"arm64", "x86_64"})
    execute_hash_elf(argv[0], arch, symbol);
  return 0;
}

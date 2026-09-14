// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <icpp.hpp>

namespace aether {
std::size_t opcode_generator(std::string_view path);
std::size_t opcret_generator(std::string_view path);
} // namespace aether

namespace {

bool load_libraries(const fs::path &thisdir, bool force = true) {
  // std::string_view type{"build-Debug"};
  std::string_view type{"build-Release"};

  // load AetherBinary
  auto vm_dir = thisdir.parent_path();
  auto binary_dir = vm_dir.parent_path() / "AetherBinary";
  auto libbinary = binary_dir / type / "libAetherBinary.dylib";
  if (!icpp::load_library(libbinary.string())) {
    if (force) {
      std::println("Failed to load {}", libbinary.string());
      return false;
    }
  }

  // load AetherVM
  auto libvm = vm_dir / type / "libAetherVM.dylib";
  if (!icpp::load_library(libvm.string())) {
    if (force) {
      std::println("Failed to load {}", libvm.string());
      return false;
    }
  }

  return true;
}

int generate_opcode(const fs::path &thisdir, std::string_view path) {
  if (!load_libraries(thisdir))
    return -1;

  auto count = aether::opcode_generator(path);
  std::println("Total generated {} native instructions.", count);
  return 0;
}

int generate_opcode_ret(const fs::path &thisdir, std::string_view path) {
  // the dependent symbol has been preloaded by ios/build.cc
  // load_libraries(const fs::path &thisdir, false);

  auto count = aether::opcret_generator(path);
  std::println("Total generated {} native instructions.", count);
  return 0;
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    std::println("Usage: {} /path/to/output", argv[0]);
    return -1;
  }

  auto thisdir = fs::absolute(argv[0]).parent_path();
  auto path = std::string_view{argv[1]};

  if (path.ends_with(".opc"))
    return generate_opcode(thisdir, path);

  if (path.ends_with(".opc.ret"))
    return generate_opcode_ret(thisdir, path);

  std::println("Invalid file extension type: {}", path);
  return -1;
}

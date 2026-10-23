// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <icpp.hpp>

#include "../include/Utils.h"

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

} // namespace

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    std::println("Usage: {} /path/to/file", argv[0]);
    return -1;
  }

  auto thisdir = fs::absolute(argv[0]).parent_path();
  if (!load_libraries(thisdir))
    return -1;

  constexpr std::string_view ext{".aevm"};
  auto path = std::string_view{argv[1]};
  auto result = 0;
  if (path.ends_with(ext))
    aether::dump_aevm(path);
  else
    result = aether::encode_aevm(path, std::string(path) + ext.data()) ? 0 : -1;
  return result;
}

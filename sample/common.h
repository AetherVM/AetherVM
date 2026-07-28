// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

// If built AetherVM successfully, the file layout should be:
// ROOT
// ---AetherBinary
// ------build-Release
// ------build-llvm
// ------------install
// ---AetherVM
// ------build-Release
//
// An example of running a sample script:
// icpp -I../AetherBinary/include aarch64-nop.cc

#include "../include/AetherVM.h"

#include <Disassembler.h>

#include <icpp.hpp>

namespace {

#if __APPLE__
const char *llvm_name = "libLLVM.dylib";
const char *binary_name = "libAetherBinary.dylib";
const char *vm_name = "libAetherVM.dylib";
const char *lib_dir = "lib";
#elif __linux__
const char *llvm_name = "libLLVM.so";
const char *binary_name = "libAetherBinary.so";
const char *vm_name = "libAetherVM.so";
const char *lib_dir = "lib";
#else
const char *llvm_name = "LLVM-22.dll";
const char *binary_name = "AetherBinary.dll";
const char *vm_name = "AetherVM.dll";
const char *lib_dir = "bin";
#endif

bool load_libraries(std::string_view script_file) {
  auto script_path = fs::absolute(script_file);
  auto script_dir = script_path.parent_path();
  auto vm_dir = script_dir.parent_path();
  auto binary_dir = vm_dir.parent_path() / "AetherBinary";
  // load LLVM
  auto libllvm = binary_dir / "build-llvm" / "install" / lib_dir / llvm_name;
  if (!icpp::load_library(libllvm.string())) {
    std::println("Failed to load {}", libllvm.string());
    return false;
  }
  for (std::string_view type :
       {"build-Debug", "build-RelWithDebInfo", "build-Release"}) {
    auto libvm = vm_dir / type / vm_name;
    if (fs::exists(libvm)) {
      // load AetherBinary
      auto libbinary = binary_dir / type / binary_name;
      if (!icpp::load_library(libbinary.string())) {
        std::println("Failed to load {}", libbinary.string());
        return false;
      }
      // load AetherVM
      if (icpp::load_library(libvm.string()))
        break;
      std::println("Failed to load {}", libvm.string());
      return false;
    }
  }
  return true;
}

std::vector<std::string> string_split(std::string_view str,
                                      std::string_view split) {
  std::vector<std::string> result;
  size_t start = 0;
  size_t end = str.find(split, start);
  while (end != std::string_view::npos) {
    // Extract the substring from 'start' to 'end'
    result.emplace_back(str.substr(start, end - start));

    // Move 'start' past the splitter
    start = end + split.length();

    // Find the next occurrence of the splitter
    end = str.find(split, start);
  }
  // Add the last part of the string (or the whole string if no splitter was
  // found)
  result.emplace_back(str.substr(start));
  return result;
}

std::vector<uint8_t> assemble(aether::Machine *mach, std::string_view codes) {
  std::vector<uint8_t> result;
  aether::Disassembler diser{aether::Binary::arch(mach->archType())};
  for (auto &code : string_split(codes, "\n")) {
    uint8_t opcode[20];
    auto info = diser.assemble(code.c_str(), opcode);
    if (opcode[0])
      result.append_range(std::span<const uint8_t>{&opcode[1], opcode[0]});
    else
      std::println("Failed to assemble: {}.", info);
  }
  return result;
}

} // namespace

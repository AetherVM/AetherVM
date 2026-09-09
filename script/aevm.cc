// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "aebi/include/AetherBinary.h"
#include "include/AetherVM.h"

#include <icpp.hpp>

namespace {

#if __APPLE__
const char *binary_name = "libAetherBinary.dylib";
const char *vm_name = "libAetherVM.dylib";
const char *lib_dir = "lib";
#elif __LINUX__
const char *binary_name = "libAetherBinary.so";
const char *vm_name = "libAetherVM.so";
const char *lib_dir = "lib";
#else
const char *binary_name = "AetherBinary.dll";
const char *vm_name = "AetherVM.dll";
const char *lib_dir = "bin";
#endif

bool load_libraries(std::string_view script_file) {
  auto script_path = fs::absolute(script_file);
  auto script_dir = script_path.parent_path();
  // load AetherBinary
  auto libbinary = script_dir / "aebi" / lib_dir / binary_name;
  if (!icpp::load_library(libbinary.string())) {
    std::println("Failed to load {}", libbinary.string());
    return false;
  }
  // load AetherVM
  auto libvm = script_dir / lib_dir / vm_name;
  if (!icpp::load_library(libvm.string())) {
    std::println("Failed to load {}", libvm.string());
    return false;
  }
  return true;
}

// Parses hex string into a vector of bytes
// (e.g. "1f2003d5" -> {0x1f, 0x20, 0x03, 0xd5})
std::vector<uint8_t> parse_opcode(std::string_view hex) {
  std::vector<uint8_t> opcodes;
  if (hex.length() % 2 != 0) {
    std::println(stderr, "Warning: hex opcode string length is odd, ignoring "
                         "trailing character.");
  }

  opcodes.reserve(hex.length() / 2);
  for (size_t i = 0; i + 1 < hex.length(); i += 2) {
    uint8_t byte = 0;
    auto [ptr, ec] =
        std::from_chars(hex.data() + i, hex.data() + i + 2, byte, 16);
    if (ec == std::errc{}) {
      opcodes.push_back(byte);
    } else {
      std::println(stderr, "Error parsing hex byte at offset {}: '{}'", i,
                   hex.substr(i, 2));
      break;
    }
  }
  return opcodes;
}

// Parses register string into a map (e.g. "x0=1;x2=2;x3=0xa" ->
// {"x0": 1, "x2": 2, "x3": 10})
std::map<std::string, uint64_t> parse_register(std::string_view reg_str) {
  std::map<std::string, uint64_t> regs;

  // Split string by semicolon ';'
  for (auto item : reg_str | std::views::split(';')) {
    std::string_view kv{item.begin(), item.end()};
    if (kv.empty())
      continue;

    auto eq_pos = kv.find('=');
    if (eq_pos == std::string_view::npos) {
      std::println(stderr, "Invalid register format '{}', missing '='", kv);
      continue;
    }

    std::string_view name = kv.substr(0, eq_pos);
    std::string_view val_str = kv.substr(eq_pos + 1);

    // Trim whitespace
    while (!name.empty() && std::isspace(name.front()))
      name.remove_prefix(1);
    while (!name.empty() && std::isspace(name.back()))
      name.remove_suffix(1);
    while (!val_str.empty() && std::isspace(val_str.front()))
      val_str.remove_prefix(1);
    while (!val_str.empty() && std::isspace(val_str.back()))
      val_str.remove_suffix(1);

    if (name.empty() || val_str.empty())
      continue;

    // Detect base (supports decimal "10" and hex "0xa")
    int base = 10;
    if (val_str.starts_with("0x"sv) || val_str.starts_with("0X"sv)) {
      base = 16;
      val_str.remove_prefix(2);
    }

    uint64_t val = 0;
    auto [ptr, ec] = std::from_chars(
        val_str.data(), val_str.data() + val_str.size(), val, base);
    if (ec == std::errc{}) {
      regs[std::string(name)] = val;
    } else {
      std::println(stderr, "Failed to parse value for register '{}': '{}'",
                   name, val_str);
    }
  }

  return regs;
}

void set_register_arm64(aether::BinaryEngine *engine,
                        std::map<std::string, uint64_t> &regs) {
  std::map<std::string, aether::Register> n2r;
  n2r["fp"] = aether::Register::FP;
  n2r["lr"] = aether::Register::LR;
  n2r["sp"] = aether::Register::SP;
  for (int i = 0; i < 32; i++)
    n2r[std::format("x{}", i)] =
        (aether::Register)((int)aether::Register::X0 + i);
  for (int i = 0; i < 32; i++)
    n2r[std::format("q{}", i)] =
        (aether::Register)((int)aether::Register::Q0 + i);
  for (auto &[reg, val] : regs) {
    auto found = n2r.find(reg);
    if (found == n2r.end())
      std::println(stderr, "Bad register name '{}'", reg);
    else
      engine->setRegister(found->second, aether::RegisterValue{.u8 = val});
  }
}

void set_register_x64(aether::BinaryEngine *engine,
                      std::map<std::string, uint64_t> &regs) {
  std::map<std::string, aether::Register> n2r;

  n2r["rbp"] = aether::Register::RBP;
  n2r["rsp"] = aether::Register::RSP;
  n2r["rax"] = aether::Register::RAX;
  n2r["rbx"] = aether::Register::RBX;
  n2r["rcx"] = aether::Register::RCX;
  n2r["rdx"] = aether::Register::RDX;
  n2r["rsi"] = aether::Register::RSI;
  n2r["rdi"] = aether::Register::RDI;
  n2r["r8"] = aether::Register::R8;
  n2r["r9"] = aether::Register::R9;
  n2r["r10"] = aether::Register::R10;
  n2r["r11"] = aether::Register::R11;
  n2r["r12"] = aether::Register::R12;
  n2r["r13"] = aether::Register::R13;
  n2r["r14"] = aether::Register::R14;
  n2r["r15"] = aether::Register::R15;
  n2r["rflags"] = aether::Register::RFLAGS;

  for (int i = 0; i < 8; ++i) {
    n2r[std::format("st{}", i)] = static_cast<aether::Register>(
        static_cast<int>(aether::Register::ST0) + i);
    n2r[std::format("mm{}", i)] = static_cast<aether::Register>(
        static_cast<int>(aether::Register::MM0) + i);
  }

  for (int i = 0; i < 32; ++i) {
    n2r[std::format("xmm{}", i)] = static_cast<aether::Register>(
        static_cast<int>(aether::Register::XMM0) + i);
    n2r[std::format("ymm{}", i)] = static_cast<aether::Register>(
        static_cast<int>(aether::Register::XMM0) + i);
    n2r[std::format("zmm{}", i)] = static_cast<aether::Register>(
        static_cast<int>(aether::Register::XMM0) + i);
  }

  for (auto &[name, val] : regs) {
    auto found = n2r.find(name);
    if (found == n2r.end())
      std::println(stderr, "Bad register name '{}'", name);
    else
      engine->setRegister(found->second, aether::RegisterValue{.u8 = val});
  }
}

void emu_start(aether::Machine *mach, std::span<const uint8_t> opcode,
               std::map<std::string, uint64_t> &regs, bool debug) {
  auto set_register =
      mach->archType() == aether::ARM64 ? set_register_arm64 : set_register_x64;
  aether::EventConfig conf;
  conf.debug = debug;

  aether::BinaryEngine engine{mach, conf};
  set_register(&engine, regs);
  if (!engine.execute(opcode))
    std::println("Failed to execute the input opcode.");
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    std::println(
        R"(Usage: {} [-arch x64|arm64] [-bin hex-opcodes] [-reg init-list] [-debug]
-arch  : if omitted then the current host architecture will be applied
-bin   : raw opcode in hex format
-reg   : register initial name=val list split with ';'
-debug : start the internal gdb-remote debug server

e.g.:
  icpp aevm.cc -arch arm64 -bin 1f2003d51f2003d51f2003d5
  icpp aevm.cc -arch arm64 -bin 1f2003d51f2003d51f2003d5 -reg "x0=1;x1=2;x2=0xa;x3=0xb" -debug
  icpp aevm.cc -arch x64 -bin 909090
  icpp aevm.cc -arch x64 -bin 909090 -reg "rdi=1;rsi=2;rdx=0xa;rcx=0xb" -debug)",
        argv[0]);
    return 0;
  }

  if (!load_libraries(argv[0]))
    return -1;

  aether::MachineARM64 arm64;
  aether::MachineX86 x64;
  aether::Machine *mach =
#if __ARM64__
      &arm64;
#else
      &x64;
#endif

  bool debug = false;
  std::vector<uint8_t> opcodes;
  std::map<std::string, uint64_t> regs;
  for (int i = 1; i < argc; i++) {
    std::string_view opt{argv[i]};
    if (opt == "-arch"sv) {
      i++;

      std::string_view arch{argv[i]};
      if (arch == "arm64"sv)
        mach = &arm64;
      else if (arch == "x64"sv)
        mach = &x64;
      else
        std::println(
            "Unsupported architecture name '{}', only x64 or arm64 is valid.",
            arch);
    } else if (opt == "-debug"sv) {
      debug = true;
    } else if (opt == "-bin"sv) {
      i++;

      opcodes = parse_opcode(argv[i]);
    } else if (opt == "-reg"sv) {
      i++;

      regs = parse_register(argv[i]);
    } else {
      std::println("Unsupported input argument '{}'", argv[i]);
    }
  }

  std::println("Start emulating...");
  emu_start(mach, opcodes, regs, debug);
  std::println("Done.");
  return 0;
}

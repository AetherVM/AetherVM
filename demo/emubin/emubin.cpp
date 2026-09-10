// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <AetherBinary.h>
#include <AetherVM.h>

#include <charconv>
#include <filesystem>
#include <map>
#include <print>
#include <ranges>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

using namespace std::literals::string_view_literals;

namespace {

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

void emu_start(aether::Binary *bin, addr_t entry,
               std::map<std::string, uint64_t> &regs, bool debug) {
  auto set_register =
      bin->archType() == aether::ARM64 ? set_register_arm64 : set_register_x64;
  aether::EventConfig conf;
  conf.debug = debug;

  aether::BinaryEngine engine{bin, conf};
  set_register(&engine, regs);
  if (!engine.execute(entry))
    std::println("Failed to execute the input opcode.");
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    std::println(
        R"(Usage: {} -bin /path/to/binary [-entry hex-rva] [-reg init-list] [-debug]
-bin   : the binary path in PE/ELF/MachO format
-entry : the rva value in hexidecimal format
-reg   : register initial name=val list split with ';'
-debug : start the internal gdb-remote debug server

e.g.:
  icpp aevm.cc -bin /path/to/binary -entry 0x1000
  icpp aevm.cc -bin /path/to/binary -entry 0x1000 -reg "x0=1;x1=2;x2=0xa;x3=0xb" -debug
  icpp aevm.cc -bin /path/to/binary -entry 0x1000 -reg "rdi=1;rsi=2;rdx=0xa;rcx=0xb" -debug)",
        argv[0]);
    return 0;
  }

  bool debug = false;
  uint64_t entry = 0;
  aether::Binary *bin = nullptr;
  std::map<std::string, uint64_t> regs;
  for (int i = 1; i < argc; i++) {
    std::string_view opt{argv[i]};
    if (opt == "-bin"sv) {
      i++;

      bin = aether::New(argv[i]);
      if (!bin) {
        std::println("Failed to load file '{}'.", argv[i]);
        return -1;
      }
    } else if (opt == "-debug"sv) {
      debug = true;
    } else if (opt == "-entry"sv) {
      i++;

      entry = std::stoull(argv[i]);
    } else if (opt == "-reg"sv) {
      i++;

      regs = parse_register(argv[i]);
    } else {
      std::println("Unsupported input argument '{}'", argv[i]);
    }
  }
  if (!bin) {
    std::println("No binary file specified.");
    return -1;
  }

  std::println("Start emulating...");
  emu_start(bin, entry, regs, debug);
  aether::Delete(bin);
  std::println("Done.");
  return 0;
}

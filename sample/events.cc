// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "common.h"
#include "elf_hash.c"

namespace {

// Helper visitor for the Unified Callback
struct EventVisitor {
  aether::EventResult operator()(const aether::EventLift &ev) const {
    std::cout << "[UNIFIED ][LIFT] Type: " << static_cast<int>(ev.type)
              << " | PC: 0x" << std::hex << ev.addr << std::dec
              << " | Size: " << ev.size << " bytes\n"
              << " | Handler/Symbol: " << (ev.name.empty() ? "<none>" : ev.name)
              << "\n";
    return aether::EventResult::Continue;
  }

  aether::EventResult operator()(const aether::EventMemory &ev) const {
    std::cout << "[UNIFIED ][MEMORY] Type: " << static_cast<int>(ev.type)
              << " | PC: 0x" << std::hex << ev.addr << std::dec
              << " | Size: " << ev.size << " bytes\n"
              << " | Value: 0x" << std::hex << ev.value.u8 << std::endl;
    return aether::EventResult::Continue;
  }

  aether::EventResult operator()(const aether::EventHyperCall &ev) const {
    std::cout << "[UNIFIED ][HYPERCALL] Type: " << static_cast<int>(ev.type)
              << " | PC: 0x" << std::hex << ev.addr << std::dec;

    if (ev.type == aether::EventType::SyscallBefore ||
        ev.type == aether::EventType::SyscallAfter) {
      std::cout << " | Syscall No: " << ev.sysno << "\n";
      if (ev.type == aether::EventType::SyscallBefore &&
          ev.sysno == 60 /* SYS_exit */) {
        std::cout << "[UNIFIED ] Intercepted exit syscall, setting result to "
                     "Processed.\n";
        return aether::EventResult::Processed;
      }
    } else {
      std::cout << " | Target Addr: 0x" << std::hex << ev.target << std::dec
                << "\n";
    }
    return aether::EventResult::Continue;
  }

  aether::EventResult operator()(const aether::EventRuntime &ev) const {
    std::cout << "[UNIFIED ][RUNTIME] Type: " << static_cast<int>(ev.type)
              << " | PC: 0x" << std::hex << ev.addr << std::dec << "\n";

    if (ev.type == aether::EventType::InvalidInsn) {
      std::cerr << "[UNIFIED ] Error: Hit invalid instruction, terminating "
                   "execution.\n";
      return aether::EventResult::Terminate;
    }
    return aether::EventResult::Continue;
  }
};

void log_hash_elf(std::string_view arch, uint32_t hash) {
  std::cout << std::setfill(' ') << std::setw(8) << arch << ": elf_hash = 0x"
            << std::hex << hash << std::endl;
}

} // namespace

int main(int argc, const char *argv[]) {
  if (!load_libraries(argv[0]))
    return -1;

  std::string_view symbol = "AetherVM";
  log_hash_elf(
      "host", elf_hash(reinterpret_cast<const unsigned char *>(symbol.data())));

  // 1. Enable event categories in EventConfig bitfield
  aether::EventConfig cfg{};
  cfg.lift = true;    // LiftBefore, LiftAfter
  cfg.memory = true;  // MemRead, MemWrite, MemMap, MemUnmap, MemProtect
  cfg.func = true;    // FuncBefore, FuncAfter
  cfg.block = true;   // BlockBefore, BlockAfter
  cfg.insn = true;    // InsnBefore, InsnAfter
  cfg.syscall = true; // SyscallBefore, SyscallAfter
  cfg.trap = true;    // TrapBefore, TrapAfter
  cfg.bridge = true;  // HostBridgeBefore, HostBridgeAfter

  // 2. Instantiate BinaryEngine
  // emulate the call to 'elf_hash(name.c_str())' in the obj file
  auto arch = "arm64";
  auto dir = fs::absolute(argv[0]).parent_path();
  auto obj = (dir / std::format("elf_hash.{}.obj", arch)).string();
  auto bin = aether::New(obj.c_str());
  auto &func = bin->functions().begin()->second;
  aether::BinaryEngine engine(bin, cfg);

  // 3. Option A: Unified Callback via std::visit
  engine.registerCallback([](aether::Event &event) -> aether::EventResult {
    return std::visit(EventVisitor{}, event);
  });

  // 4. Option B: Granular Callback via std::get_if (identical logic, distinct
  // prefix)
  engine.registerCallback([](aether::Event &event) -> aether::EventResult {
    if (auto *liftEv = std::get_if<aether::EventLift>(&event)) {
      std::cout << "[GRANULAR][LIFT] Type: " << static_cast<int>(liftEv->type)
                << " | PC: 0x" << std::hex << liftEv->addr << std::dec
                << " | Size: " << liftEv->size << " bytes\n"
                << " | Handler/Symbol: "
                << (liftEv->name.empty() ? "<none>" : liftEv->name) << "\n";
      return aether::EventResult::Continue;
    }

    if (auto *memEv = std::get_if<aether::EventMemory>(&event)) {
      std::cout << "[GRANULAR][MEMORY] Type: " << static_cast<int>(memEv->type)
                << " | PC: 0x" << std::hex << memEv->addr << std::dec
                << " | Size: " << memEv->size << " bytes\n"
                << " | Value: 0x" << std::hex << memEv->value.u8 << std::endl;
      return aether::EventResult::Continue;
    }

    if (auto *hyperEv = std::get_if<aether::EventHyperCall>(&event)) {
      std::cout << "[GRANULAR][HYPERCALL] Type: "
                << static_cast<int>(hyperEv->type) << " | PC: 0x" << std::hex
                << hyperEv->addr << std::dec;

      if (hyperEv->type == aether::EventType::SyscallBefore ||
          hyperEv->type == aether::EventType::SyscallAfter) {
        std::cout << " | Syscall No: " << hyperEv->sysno << "\n";
        if (hyperEv->type == aether::EventType::SyscallBefore &&
            hyperEv->sysno == 60 /* SYS_exit */) {
          std::cout << "[GRANULAR] Intercepted exit syscall, setting result to "
                       "Processed.\n";
          return aether::EventResult::Processed;
        }
      } else {
        std::cout << " | Target Addr: 0x" << std::hex << hyperEv->target
                  << std::dec << "\n";
      }
      return aether::EventResult::Continue;
    }

    if (auto *runtimeEv = std::get_if<aether::EventRuntime>(&event)) {
      std::cout << "[GRANULAR][RUNTIME] Type: "
                << static_cast<int>(runtimeEv->type) << " | PC: 0x" << std::hex
                << runtimeEv->addr << std::dec << "\n";

      if (runtimeEv->type == aether::EventType::InvalidInsn) {
        std::cerr << "[GRANULAR] Error: Hit invalid instruction, terminating "
                     "execution.\n";
        return aether::EventResult::Terminate;
      }
      return aether::EventResult::Continue;
    }

    return aether::EventResult::Continue;
  });

  // 5. Setup arguments and start emulation
  // initialize the first argument
  engine.setRegister(aether::Register::X0, {.str = symbol.data()});
  // call elf_hash function
  if (engine.execute(func.start))
    log_hash_elf(arch, engine.getRegister(aether::Register::X0)->u4);
  else
    std::cerr << arch << ": execute elf_hash failed" << std::endl;

  aether::Delete(bin);
  return 0;
}

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <span>
#include <vector>

#include "Event.h"
#include "Register.h"

#ifdef _WIN32
#ifdef AETHER_DLLIMPL
#define __AETHER_VMAPI__ __declspec(dllexport)
#else
#define __AETHER_VMAPI__ __declspec(dllimport)
#endif // end of AETHER_DLLIMPL
#else
#define __AETHER_VMAPI__ __attribute__((visibility("default")))
#endif // end of _WIN32

namespace aether {

// should be the same as remill's definition
using addr_t = uint64_t;

class Machine;
class Binary;
struct BinaryEngineImpl;

class __AETHER_VMAPI__ BinaryEngine {
public:
  // Construct a raw virtual cpu context from an architecture instance, what you
  // can operate with is barely raw binary code, which is useful for shellcode
  // analysis and emulation, or a virtual cpu loop like in ICPP which has
  // already got its own analysis context and what it needs is nothing but
  // executing instructions.
  BinaryEngine(const Machine *mach);

  // Construct a raw virtual cpu context from a binary instance, which can
  // provide all the functions within it to be operated with, like lifting,
  // instrumenting, and analyzing.
  BinaryEngine(const Binary *bin);

  virtual ~BinaryEngine();

  // Configure the event system.
  void setConfig(EventConfig eventcfg);

  // Execute raw machine opcodes.
  bool execute(std::span<const uint8_t> raw);

  // Execute opcodes of target which belongs to the current attached binary, or
  // any other mapMemory returned vm address.
  bool execute(addr_t target);

  // Execute the main function of an executable binary file which the current
  // attached binary should be. The subclass should override recursiveLoad to do
  // stuffs like relocation binding, segment mapping, and dependent libraries
  // loading, otherwise it fails instantly.
  virtual bool runMain();

  // Make an executable function pointer from raw machine opcodes, in cases
  // like: 1.the target of relocation is directly called by host system; 2.the
  // callback passes to host runtime; 3.you want to directly convert to a known
  // prototype and call without manually setting register contexts;
  const void *makeExecutable(std::span<const uint8_t> raw);

  // Get the readonly pointer of a specified register, return nullptr if reg is
  // not invalid for this instance.
  const RegisterValue *getRegister(Register reg);

  // Set the value of a specified register.
  // Don't reset SP register which is automatically set for each thread.
  bool setRegister(Register reg, RegisterValue val);

  // Map memory for VM guest, return 0 means OOM.
  addr_t mapMemory(size_t size);

  // Read memory from VM guest, return empty if addr is not valid VM memory.
  std::vector<uint8_t> readMemory(addr_t addr, size_t size);
  uint64_t readUInt64(addr_t addr);

  // Write memory to VM guest, return false if addr is not valid VM memory.
  bool writeMemory(addr_t addr, std::span<const uint8_t> buff);

  // Add an event callback.
  // This's not thread-safe, only register any callbacks before executing the
  // guest code.
  int registerCallback(EventCallback callback);

protected:
  // For MachOEngine, ELFEngine, and PEEngine to implement....
  virtual bool recursiveLoad() { return false; }

  // Lift the opcodes for engine runtime.
  void liftOpcodes(const Binary *bin, std::span<const uint8_t> opcodes);

  // Map, Lift and Orchestrate the bin into guest memory region.
  void orchBinary(const Binary *bin, addr_t addend);

protected:
  // Make it directly visible for subclasses.
  const Machine *m_machine = nullptr;
  const Binary *m_binary = nullptr;

private:
  // Internal engine implementation.
  std::unique_ptr<BinaryEngineImpl> m_impl;
};

} // namespace aether

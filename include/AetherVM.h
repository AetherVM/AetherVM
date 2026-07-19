// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <AetherBinary.h>
#include <span>

#include "Event.h"
#include "Register.h"

namespace aether {

class __AETHER_API__ BinaryEngine {
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

  // Execute raw machine opcodes.
  bool execute(std::span<uint8_t> raw);

  // Execute opcodes of target which belongs to the current attached binary.
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
  const void *makeExecutable(std::span<uint8_t> raw);

  // Get the readonly pointer of a specified register.
  const RegisterValue *getRegister(Register reg);

  // Set the value of a specified register.
  void setRegister(Register reg, RegisterValue val);

  // Add an event callback.
  int registerCallback(EventCallback callback);

protected:
  // For MachOEngine, ELFEngine, and PEEngine to implement....
  virtual bool recursiveLoad() { return false; }

protected:
  // Make it directly visible for subclasses.
  const Machine *m_machine = nullptr;
  const Binary *m_binary = nullptr;

private:
  // Internal implementation.
  void *m_impl = nullptr;
};

} // namespace aether

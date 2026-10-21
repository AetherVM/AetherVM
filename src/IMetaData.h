// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <cstdint>
namespace aether {

struct IMetaData {
  static constexpr uint32_t kMagic = 'MVEA'; // AEVM
  static constexpr uint32_t kVersion = 5;

  // the target platform has write-execute memory support
  static constexpr uint32_t kFlag_haswx = 0x1;

  // Operand fields use fixed-width values and register state offsets, never
  // pointers or variable-length Remill strings.
  struct Expression {
    enum Kind : uint8_t {
      kInvalid,
      kOperation,
      kRegister,
      kConstant,
    };

    struct Operation {
      uint32_t lhs;
      uint32_t rhs;
    };

    struct Register {
      uint16_t offset;
      uint16_t size;
    };

    struct Constant {
      uint32_t low;
      uint32_t high;
    };

    union Payload {
      Operation operation;
      Register reg;
      Constant constant;
    } payload;

    uint8_t kind;
    uint8_t opcode;
    uint8_t width;
    uint8_t reserved;

    static constexpr uint32_t no_child = UINT32_MAX;
  };

  struct Operand {
    struct Register {
      uint16_t offset;
      uint16_t size;
    };

    struct ShiftRegister {
      Register reg;
      uint8_t shift_size;
      uint8_t extract_size;
      uint8_t shift_first;
      uint8_t can_shift_op_size;
      uint8_t shift_op;
      uint8_t extend_op;
    };

    struct Immediate {
      uint64_t value;
      uint8_t is_signed;
    };

    struct Address {
      Register regs[3];
      int64_t scale;
      int64_t displacement;
      uint8_t address_size;
      uint8_t kind;
    };

    union Value {
      Register reg;
      ShiftRegister shift_register;
      Immediate immediate;
      Address address;
      Expression expression;
    };

    uint8_t size;
    uint8_t type;
    uint8_t action;
    Value value;
  };

  uint32_t magic;
  uint32_t version;
  uint32_t size;
  uint32_t flags;
  uint32_t off_operands;     // remill operand section offset
  uint32_t num_operands;     // number of remill operands
  uint32_t off_expressions;  // expression node section offset
  uint32_t num_expressions;  // number of expression nodes
  uint32_t off_instructions; // aot instruction section offset
  uint32_t num_instructions; // number of aot instructions
  uint32_t off_chains;       // execution chain section offset
  uint32_t num_chains;       // number of execution chains
};

} // namespace aether

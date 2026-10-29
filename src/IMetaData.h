// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <compare>
#include <cstdint>

namespace aether {

struct IMetaData {
  static constexpr uint32_t kMagic = 'MVEA'; // AEVM
  static constexpr uint32_t kVersion = 1;

  enum {
    kFlag_HasWX = 1 << 0, // writable and executable memory region
    kFlag_ARM64 = 1 << 1, // arm64 architecture
    kFlag_X64 = 1 << 2    // x64 architecture
  };

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

    uint16_t size;
    uint8_t type;
    uint8_t action;
    Value value;
  };

  // Note, when serializing for arm64 with valid aotimpl, each of Handler must
  // be aligned with 4 bytes, so that the aot implementation bytes can be
  // correctly aligned in memory at runtime to execute properly.
  struct Handler {
    uint16_t impl;    // implementation index
    uint8_t type : 4; // OpcodeHandler::Type
    uint8_t oplen : 4;
    uint8_t oprnum;
    uint8_t opcode[0];
    uint16_t operands[0];
    uint8_t aotimpl[0]; // aot implementation bytes

    const uint16_t *operand() const {
      return reinterpret_cast<const uint16_t *>(&opcode[oplen]);
    }

    const uint8_t *handler() const {
      return reinterpret_cast<const uint8_t *>(operand() + oprnum);
    }
  };

  // header
  uint32_t magic;
  uint32_t version;
  uint32_t size;
  uint32_t flags; // metadata flags, e.g., kFlag_HasWX
  // <rva, handler offset from the start of the handler section>
  uint32_t off_instruction; // instruction meta section offset
  uint32_t num_instruction; // number of instruction metas
  // remill operand list
  uint32_t off_operands; // remill operand section offset
  uint32_t num_operands; // number of remill operands
  // expression node list referenced by remill operands
  uint32_t off_expressions; // expression node section offset
  uint32_t num_expressions; // number of expression nodes
  // handler list
  uint32_t off_handlers; // handler section offset
  uint32_t num_handlers; // number of handler
  // end of header
  //
  // body of the metadata follows the header
  //
  // instruction meta
  // operand meta
  // expression meta
  // handler meta

  struct InsnMeta {
    uint32_t rva; // relative virtual address of the instruction
    uint32_t
        handler; // offset to the handler from the start of the handler section

    auto operator<=>(const InsnMeta &other) const { return rva <=> other.rva; }
  };

  const InsnMeta *insns() const {
    return reinterpret_cast<const InsnMeta *>(
        reinterpret_cast<const uint8_t *>(this) + off_instruction);
  }

  const Handler *handler(const InsnMeta *insn) const {
    return reinterpret_cast<const Handler *>(
        reinterpret_cast<const uint8_t *>(this) + off_handlers + insn->handler);
  }
};

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "OpcodeEngine.h"

namespace aether {

class Binary;

class PreEncoder {
public:
  PreEncoder(const Binary *bin, std::string_view outpath, bool haswx);
  ~PreEncoder();

  bool encode();
  static void dump(std::string_view path);

private:
  void prefetch();
  void analyzeInsnMeta();
  bool serialize();
  std::vector<std::vector<uint8_t>>
  serializeHandler(std::vector<uint32_t> &hoffsets);
  uint32_t handlerIndex(std::span<const uint8_t> opcode);

private:
  struct Handler {
    int oplen;
    union {
      const void *ptr;
      const OpcodeHandler<uint8_t> *ptr1;
      const OpcodeHandler<uint16_t> *ptr2;
      const OpcodeHandler<uint32_t> *ptr4;
      const OpcodeHandler<uint64_t> *ptr8;
      const OpcodeHandler<uint128_var_t> *ptr16;
    };
  };
  struct InsnMeta {
    uint32_t rva;     // rva of this instruction
    uint32_t handler; // instruction handler index
  };

  const Binary *m_bin;
  std::string_view m_outpath;
  bool m_haswx; // writable-executable page

  OpcodeEngine m_engine;
  std::vector<Handler> m_handlers;
  // the [start, end) index ranges of the different length opcodes
  std::pair<uint32_t, uint32_t> m_ranges[5];
  std::vector<InsnMeta> m_insnmetas;
};

} // namespace aether

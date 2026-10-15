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
  PreEncoder(const Binary *bin, std::string_view outpath);
  ~PreEncoder();

  bool encode();

private:
  void prefetch();
  void parseBasicBlocks();

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
  struct BasicBlock {
    uint32_t rva;                // rva of this basic block
    std::vector<uint32_t> insns; // instruction handler indexs
  };

  const Binary *m_bin;
  std::string_view m_outpath;
  OpcodeEngine m_engine;
  std::vector<Handler> m_handlers;
  std::vector<BasicBlock> m_basicblocks;
};

} // namespace aether

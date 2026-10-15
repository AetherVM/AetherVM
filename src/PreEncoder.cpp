// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "PreEncoder.h"

#include <AetherBinary.h>

namespace aether {

PreEncoder::PreEncoder(const Binary *bin, std::string_view outpath)
    : m_bin(bin), m_outpath(outpath) {}

PreEncoder::~PreEncoder() {}

void PreEncoder::prefetch() {
  auto &sect = m_bin->sections();
  auto &sectbuff = m_bin->sectBuffers();
  auto sectit = sect.begin();
  auto sbufit = sectbuff.begin();
  for (auto endit = sect.end(); sectit != endit; sectit++, sbufit++) {
    auto &sect = sectit->second;
    if (sect.type == TEXT) {
      auto buff = reinterpret_cast<const uint8_t *>(sbufit->second);
      m_engine.prefetch({buff, sect.size});
    }
  }

  // put all the handlers in an indexed sequence so that we can refer and
  // serialize them in a sorted manner
  auto load_handlers = [this]<typename T>(OpcodeHandlers<T> &ohandlers) {
    for (auto &handler : ohandlers.handlers)
      m_handlers.push_back(Handler{(int)sizeof(T), {.ptr = &handler}});
  };
  load_handlers(m_engine.opc1);
  load_handlers(m_engine.opc2);
  load_handlers(m_engine.opc4);
  load_handlers(m_engine.opc8);
  load_handlers(m_engine.opc16);
}

void PreEncoder::parseBasicBlocks() {}

bool PreEncoder::encode() {
  // prefetch all the opcodes into opcode engine
  prefetch();

  // parse basic blocks as the minimal execution unit
  parseBasicBlocks();

  return false;
}

} // namespace aether

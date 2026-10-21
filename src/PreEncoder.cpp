// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "PreEncoder.h"

#include <AetherBinary.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

namespace aether {

std::map<uint32_t, uint32_t> PreEncoder::m_rva2rt;

PreEncoder::PreEncoder(const Binary *bin, std::string_view outpath, bool haswx)
    : m_bin(bin), m_outpath(outpath), m_haswx(haswx) {
  // set the engine to readonly mode so that it can free all the internal
  // resources after finishing encoding
  m_engine.readonly = true;
}

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
  m_ranges[0] = std::make_pair(0, (uint32_t)m_handlers.size());
  load_handlers(m_engine.opc2);
  m_ranges[1] = std::make_pair(m_ranges[0].second, (uint32_t)m_handlers.size());
  load_handlers(m_engine.opc4);
  m_ranges[2] = std::make_pair(m_ranges[1].second, (uint32_t)m_handlers.size());
  load_handlers(m_engine.opc8);
  m_ranges[3] = std::make_pair(m_ranges[2].second, (uint32_t)m_handlers.size());
  load_handlers(m_engine.opc16);
  m_ranges[4] = std::make_pair(m_ranges[3].second, (uint32_t)m_handlers.size());
}

uint32_t PreEncoder::handlerIndex(std::span<const uint8_t> opcode) {
  constexpr uint32_t invalid = UINT32_MAX;
  auto size = opcode.size();
  if (size == 0 || size > 16)
    return invalid;

  // the handlers of each length range are sorted by opcode (std::set order)
  auto search = [&](int range, auto ptr, auto key) -> uint32_t {
    auto [start, end] = m_ranges[range];
    auto first = m_handlers.begin() + start;
    auto last = m_handlers.begin() + end;
    auto it = std::lower_bound(first, last, key,
                               [&](const Handler &h, const auto &k) {
                                 return decltype(ptr)(h.ptr)->opcode < k;
                               });
    if (it == last || !(decltype(ptr)(it->ptr)->opcode == key))
      return invalid;
    return (uint32_t)(it - m_handlers.begin());
  };

  uint8_t tmp1{0};
  uint16_t tmp2{0};
  uint32_t tmp4{0};
  uint64_t tmp8{0};
  uint128_var_t tmp16{0, 0};
  if (size == 1) {
    tmp1 = opcode[0];
    return search(0, (const OpcodeHandler<uint8_t> *)nullptr, tmp1);
  }
  if (size == 2) {
    std::memcpy(&tmp2, opcode.data(), 2);
    return search(1, (const OpcodeHandler<uint16_t> *)nullptr, tmp2);
  }
  if (size <= 4) {
    std::memcpy(&tmp4, opcode.data(), size);
    return search(2, (const OpcodeHandler<uint32_t> *)nullptr, tmp4);
  }
  if (size <= 8) {
    std::memcpy(&tmp8, opcode.data(), size);
    return search(3, (const OpcodeHandler<uint64_t> *)nullptr, tmp8);
  }
  std::memcpy(&tmp16, opcode.data(), size);
  return search(4, (const OpcodeHandler<uint128_var_t> *)nullptr, tmp16);
}

void PreEncoder::analyzeInsnMeta() {
  for (auto &fnit : m_bin->functions()) {
    auto &func = fnit.second;
    auto fnbuf = (const uint8_t *)m_bin->addrBuff(func.start);
    auto fnrva = func.start - m_bin->imageBase();
    for (auto &insn : func.insns) {
      if (insn.info.type == IDATA)
        continue;
      InsnMeta meta;
      meta.rva = fnrva + insn.fnoff;
      meta.handler = handlerIndex({fnbuf + insn.fnoff, insn.info.oplen});
      m_insnmetas.push_back(meta);
    }
  }
}

bool PreEncoder::serialize() {
  std::vector<IMetaData::Operand> operands;
  std::vector<IMetaData::Expression> expressions;
  if (!OpcodeEngine::convertOperands(operands, expressions))
    return false;

  const uint64_t off_operands = sizeof(IMetaData);
  const uint64_t off_expressions =
      off_operands + operands.size() * sizeof(IMetaData::Operand);
  const uint64_t outsize =
      off_expressions + expressions.size() * sizeof(IMetaData::Expression);
  if (outsize > std::numeric_limits<uint32_t>::max()) {
    log_print(Runtime, "Metadata size exceeds the 32-bit format limit");
    return false;
  }

  std::vector<uint8_t> outbuf(static_cast<size_t>(outsize), 0);
  IMetaData metadata{};
  metadata.magic = IMetaData::kMagic;
  metadata.version = IMetaData::kVersion;
  metadata.size = static_cast<uint32_t>(outsize);
  metadata.off_operands = static_cast<uint32_t>(off_operands);
  metadata.num_operands = static_cast<uint32_t>(operands.size());
  metadata.off_expressions = static_cast<uint32_t>(off_expressions);
  metadata.num_expressions = static_cast<uint32_t>(expressions.size());
  metadata.off_instructions = static_cast<uint32_t>(outsize);
  metadata.off_chains = static_cast<uint32_t>(outsize);
  if (m_haswx)
    metadata.flags |= IMetaData::kFlag_haswx;

  std::memcpy(outbuf.data(), &metadata, sizeof(metadata));
  if (!operands.empty()) {
    std::memcpy(outbuf.data() + off_operands, operands.data(),
                operands.size() * sizeof(IMetaData::Operand));
  }
  if (!expressions.empty()) {
    std::memcpy(outbuf.data() + off_expressions, expressions.data(),
                expressions.size() * sizeof(IMetaData::Expression));
  }

  std::ofstream ofs(std::string(m_outpath), std::ios::binary);
  if (!ofs)
    return false;
  ofs.write(reinterpret_cast<const char *>(outbuf.data()),
            static_cast<std::streamsize>(outbuf.size()));
  return ofs.good();
}

bool PreEncoder::encode() {
  // prefetch all the opcodes into opcode engine
  prefetch();

  // analyze instruction metadata
  analyzeInsnMeta();

  // serialize all the metadatas to output file
  return serialize();
}

} // namespace aether

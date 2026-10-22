// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "PreEncoder.h"
#include "Handler.h"

#include <AetherBinary.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>

namespace aether {

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

std::vector<std::vector<uint8_t>>
PreEncoder::serializeHandler(std::vector<uint32_t> &hoffsets) {
  constexpr uint16_t no_impl = UINT16_MAX;
  constexpr uint16_t operand_index_mask = 0x7fff;
  constexpr uint16_t operand_pointer_flag = 0x8000;
  const bool aotimpl = m_haswx && m_bin->archType() == ARM64;
  const auto *implementations = m_bin->archType() == ARM64
                                    ? &aether::Handler::aarch64
                                    : &aether::Handler::x86;

  std::vector<std::vector<uint8_t>> serialized;
  serialized.reserve(m_handlers.size());
  hoffsets.clear();
  hoffsets.reserve(m_handlers.size());
  uint64_t handler_section_offset = 0;
  uint32_t dynamic_handler_index = 0;

  auto fail = [&](std::string_view message) {
    hoffsets.clear();
    log_print(Runtime, "Failed to serialize opcode handler: {}", message);
    return std::vector<std::vector<uint8_t>>{};
  };

  for (const auto &source : m_handlers) {
    bool is_dynamic = false;
    auto convert = [&]<typename T>(const OpcodeHandler<T> &handler)
        -> std::optional<std::vector<uint8_t>> {
      is_dynamic = handler.type == OpcodeHandler<T>::OHT_Dynamic;
      if (handler.oplen > 15 || handler.oplen > source.oplen ||
          handler.args.size() > UINT8_MAX)
        return std::nullopt;

      uint16_t impl = no_impl;
      switch (handler.type) {
      case OpcodeHandler<T>::OHT_Remill: {
        const auto found =
            std::find_if(implementations->begin(), implementations->end(),
                         [&](const aether::Handler &candidate) {
                           return candidate.impl == handler.impl;
                         });
        size_t index;
        if (found != implementations->end()) {
          index = static_cast<size_t>(found - implementations->begin());
        } else {
          return std::nullopt;
        }
        if (index >= no_impl)
          return std::nullopt;
        impl = static_cast<uint16_t>(index);
        break;
      }
      case OpcodeHandler<T>::OHT_MCInst:
        if (handler.args.size() != 1 || handler.args[0].index >= no_impl)
          return std::nullopt;
        impl = static_cast<uint16_t>(handler.args[0].index);
        break;
      case OpcodeHandler<T>::OHT_Dynamic:
        if (aotimpl) {
          if (handler.implsize && !handler.impl)
            return std::nullopt;
          impl = 0;
        } else {
          if (dynamic_handler_index >= no_impl)
            return std::nullopt;
          impl = static_cast<uint16_t>(dynamic_handler_index);
        }
        break;
      default:
        return std::nullopt;
      }

      const size_t fixed_size = offsetof(IMetaData::Handler, opcode);
      const size_t opcode_size = handler.oplen;
      const size_t operand_bytes = handler.type == OpcodeHandler<T>::OHT_Remill
                                       ? handler.args.size() * sizeof(uint16_t)
                                       : 0;
      size_t aotimpl_offset = fixed_size + opcode_size + operand_bytes;
      if (aotimpl)
        aotimpl_offset = (aotimpl_offset + 3u) & ~size_t{3};
      const size_t code_size =
          aotimpl && handler.type == OpcodeHandler<T>::OHT_Dynamic
              ? handler.implsize
              : 0;
      const size_t record_size =
          (aotimpl ? (aotimpl_offset + code_size + 3u) & ~size_t{3}
                   : fixed_size + opcode_size + operand_bytes);

      std::vector<uint8_t> record(record_size, 0);
      IMetaData::Handler metadata{};
      metadata.impl = impl;
      metadata.type = static_cast<uint8_t>(handler.type);
      metadata.oplen = handler.oplen;
      metadata.oprnum =
          static_cast<uint8_t>(operand_bytes ? handler.args.size() : 0);
      std::memcpy(record.data(), &metadata, fixed_size);
      std::memcpy(record.data() + fixed_size, &handler.opcode, opcode_size);

      if (operand_bytes) {
        for (size_t i = 0; i < handler.args.size(); ++i) {
          const auto &arg = handler.args[i];
          if (arg.index > operand_index_mask)
            return std::nullopt;
          const uint16_t operand_index = static_cast<uint16_t>(
              arg.index | (arg.pointer ? operand_pointer_flag : 0));
          std::memcpy(record.data() + fixed_size + opcode_size +
                          i * sizeof(operand_index),
                      &operand_index, sizeof(operand_index));
        }
      }

      if (code_size) {
        std::memcpy(record.data() + aotimpl_offset, handler.impl, code_size);
      }
      return record;
    };

    std::optional<std::vector<uint8_t>> record;
    switch (source.oplen) {
    case 1:
      record = convert(*source.ptr1);
      break;
    case 2:
      record = convert(*source.ptr2);
      break;
    case 4:
      record = convert(*source.ptr4);
      break;
    case 8:
      record = convert(*source.ptr8);
      break;
    case 16:
      record = convert(*source.ptr16);
      break;
    default:
      return fail("invalid opcode width");
    }
    if (!record)
      return fail("handler fields exceed metadata limits or are unsupported");
    if (handler_section_offset > std::numeric_limits<uint32_t>::max() ||
        record->size() >
            std::numeric_limits<uint32_t>::max() - handler_section_offset)
      return fail("handler section offset exceeds metadata limits");
    hoffsets.push_back(static_cast<uint32_t>(handler_section_offset));
    handler_section_offset += record->size();
    serialized.push_back(std::move(*record));
    if (is_dynamic)
      ++dynamic_handler_index;
  }
  return serialized;
}

bool PreEncoder::serialize() {
  std::vector<IMetaData::Operand> operands;
  std::vector<IMetaData::Expression> expressions;
  if (!OpcodeEngine::convertOperands(operands, expressions))
    return false;
  std::vector<uint32_t> handler_offsets;
  auto handlers = serializeHandler(handler_offsets);
  if (!m_handlers.empty() && handlers.empty())
    return false;

  std::vector<InsnMeta> instructions = m_insnmetas;
  for (auto &instruction : instructions) {
    if (instruction.handler == UINT32_MAX)
      continue;
    if (instruction.handler >= handler_offsets.size()) {
      log_print(Runtime,
                "Instruction handler index {} has no serialized handler",
                instruction.handler);
      return false;
    }
    instruction.handler = handler_offsets[instruction.handler];
  }

  const uint64_t off_instruction = sizeof(IMetaData);
  const uint64_t off_operands =
      off_instruction + instructions.size() * sizeof(InsnMeta);
  const uint64_t off_expressions =
      off_operands + operands.size() * sizeof(IMetaData::Operand);
  const uint64_t off_handlers =
      off_expressions + expressions.size() * sizeof(IMetaData::Expression);
  if (instructions.size() > UINT32_MAX || operands.size() > UINT32_MAX ||
      expressions.size() > UINT32_MAX || handlers.size() > UINT32_MAX) {
    log_print(Runtime,
              "Metadata section count exceeds the 32-bit format limit");
    return false;
  }
  uint64_t outsize = off_handlers;
  for (const auto &handler : handlers)
    outsize += handler.size();
  if (outsize > std::numeric_limits<uint32_t>::max()) {
    log_print(Runtime, "Metadata size exceeds the 32-bit format limit");
    return false;
  }

  std::vector<uint8_t> outbuf(static_cast<size_t>(outsize), 0);
  IMetaData metadata{};
  metadata.magic = IMetaData::kMagic;
  metadata.version = IMetaData::kVersion;
  metadata.size = static_cast<uint32_t>(outsize);
  metadata.off_instruction = static_cast<uint32_t>(off_instruction);
  metadata.num_instruction = static_cast<uint32_t>(instructions.size());
  metadata.off_operands = static_cast<uint32_t>(off_operands);
  metadata.num_operands = static_cast<uint32_t>(operands.size());
  metadata.off_expressions = static_cast<uint32_t>(off_expressions);
  metadata.num_expressions = static_cast<uint32_t>(expressions.size());
  metadata.off_handlers = static_cast<uint32_t>(off_handlers);
  metadata.num_handlers = static_cast<uint32_t>(handlers.size());

  std::memcpy(outbuf.data(), &metadata, sizeof(metadata));
  if (!instructions.empty()) {
    std::memcpy(outbuf.data() + off_instruction, instructions.data(),
                instructions.size() * sizeof(InsnMeta));
  }
  if (!operands.empty()) {
    std::memcpy(outbuf.data() + off_operands, operands.data(),
                operands.size() * sizeof(IMetaData::Operand));
  }
  if (!expressions.empty()) {
    std::memcpy(outbuf.data() + off_expressions, expressions.data(),
                expressions.size() * sizeof(IMetaData::Expression));
  }
  size_t handler_offset = static_cast<size_t>(off_handlers);
  for (const auto &handler : handlers) {
    std::memcpy(outbuf.data() + handler_offset, handler.data(), handler.size());
    handler_offset += handler.size();
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

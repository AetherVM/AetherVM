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
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <string>

namespace aether {

namespace {

const char *operandTypeName(uint8_t type) {
  using Operand = remill::Operand;
  switch (static_cast<Operand::Type>(type)) {
  case Operand::kTypeInvalid:
    return "invalid";
  case Operand::kTypeRegister:
    return "register";
  case Operand::kTypeShiftRegister:
    return "shift-register";
  case Operand::kTypeImmediate:
    return "immediate";
  case Operand::kTypeAddress:
    return "address";
  case Operand::kTypeExpression:
    return "expression";
  case Operand::kTypeRegisterExpression:
    return "register-expression";
  case Operand::kTypeImmediateExpression:
    return "immediate-expression";
  case Operand::kTypeAddressExpression:
    return "address-expression";
  default:
    return "unknown";
  }
}

const char *handlerTypeName(uint8_t type) {
  using Handler = OpcodeHandler<uint8_t>;
  switch (static_cast<Handler::Type>(type)) {
  case Handler::OHT_Remill:
    return "Remill";
  case Handler::OHT_MCInst:
    return "MCInst";
  case Handler::OHT_Dynamic:
    return "Dynamic";
  case Handler::OHT_Prebuit:
    return "Prebuilt";
  case Handler::OHT_PrebuiltMapped:
    return "PrebuiltMapped";
  default:
    return "unknown";
  }
}

const char *expressionKindName(uint8_t kind) {
  switch (kind) {
  case IMetaData::Expression::kInvalid:
    return "invalid";
  case IMetaData::Expression::kOperation:
    return "operation";
  case IMetaData::Expression::kRegister:
    return "register";
  case IMetaData::Expression::kConstant:
    return "constant";
  default:
    return "unknown";
  }
}

} // namespace

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
    auto convert = [&]<typename T>(const OpcodeHandler<T> &handler)
        -> std::optional<std::vector<uint8_t>> {
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
          impl = handler.implsize;
        } else {
          if (dynamic_handler_index >= no_impl)
            return std::nullopt;
          impl = static_cast<uint16_t>(dynamic_handler_index++);
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
  metadata.flags = 0;
  metadata.off_instruction = static_cast<uint32_t>(off_instruction);
  metadata.num_instruction = static_cast<uint32_t>(instructions.size());
  metadata.off_operands = static_cast<uint32_t>(off_operands);
  metadata.num_operands = static_cast<uint32_t>(operands.size());
  metadata.off_expressions = static_cast<uint32_t>(off_expressions);
  metadata.num_expressions = static_cast<uint32_t>(expressions.size());
  metadata.off_handlers = static_cast<uint32_t>(off_handlers);
  metadata.num_handlers = static_cast<uint32_t>(handlers.size());
  if (m_haswx)
    metadata.flags |= IMetaData::kFlag_HasWX;

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

void PreEncoder::dump(std::string_view path) {
  std::ifstream input(std::string(path), std::ios::binary | std::ios::ate);
  if (!input) {
    std::cerr << "Unable to open metadata file: " << path << '\n';
    return;
  }

  const auto file_end = input.tellg();
  if (file_end < 0 || static_cast<uint64_t>(file_end) < sizeof(IMetaData)) {
    std::cerr << "Metadata file is too small to contain its header: " << path
              << '\n';
    return;
  }
  if (static_cast<uint64_t>(file_end) > UINT32_MAX) {
    std::cerr << "Metadata file exceeds the 32-bit format size limit: " << path
              << '\n';
    return;
  }
  const auto file_size = static_cast<size_t>(file_end);
  std::vector<uint8_t> data(file_size);
  input.seekg(0);
  if (!input.read(reinterpret_cast<char *>(data.data()),
                  static_cast<std::streamsize>(data.size()))) {
    std::cerr << "Unable to read metadata file: " << path << '\n';
    return;
  }

  auto readAt = [&](uint64_t offset, auto &value) {
    if (offset > data.size() || sizeof(value) > data.size() - offset)
      return false;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return true;
  };
  auto printBytes = [&](uint64_t offset, uint64_t length,
                        std::string_view label) {
    std::cout << "  " << label << " (" << length << " bytes):\n";
    constexpr size_t bytes_per_line = 16;
    for (uint64_t i = 0; i < length; i += bytes_per_line) {
      const auto line_size =
          static_cast<size_t>(std::min<uint64_t>(bytes_per_line, length - i));
      std::cout << "    +" << std::hex << std::setw(8) << std::setfill('0')
                << offset + i << ":";
      for (size_t j = 0; j < line_size; ++j)
        std::cout << ' ' << std::setw(2)
                  << static_cast<unsigned>(data[offset + i + j]);
      std::cout << std::dec << std::setfill(' ') << '\n';
    }
  };

  IMetaData metadata{};
  if (!readAt(0, metadata)) {
    std::cerr << "Unable to read metadata header: " << path << '\n';
    return;
  }

  std::cout << "AetherVM metadata: " << path << '\n'
            << "Header:\n"
            << "  magic: ";
  if (metadata.magic == IMetaData::kMagic)
    std::cout << "AEVM";
  else
    std::cout << "unknown";
  std::cout << " (0x" << std::hex << metadata.magic << std::dec << ")\n"
            << "  version: " << metadata.version << '\n'
            << "  declared size: " << metadata.size << " bytes\n"
            << "  actual size: " << file_size << " bytes\n"
            << "  flags: 0x" << std::hex << metadata.flags << std::dec << '\n'
            << "  instructions: offset " << metadata.off_instruction
            << ", count " << metadata.num_instruction << '\n'
            << "  operands: offset " << metadata.off_operands << ", count "
            << metadata.num_operands << '\n'
            << "  expressions: offset " << metadata.off_expressions
            << ", count " << metadata.num_expressions << '\n'
            << "  handlers: offset " << metadata.off_handlers << ", count "
            << metadata.num_handlers << '\n';

  if (metadata.magic != IMetaData::kMagic) {
    std::cerr << "Invalid metadata magic; remaining bytes are not decoded\n";
    printBytes(0, data.size(), "file");
    return;
  }
  if (metadata.version != IMetaData::kVersion) {
    std::cerr << "Unsupported metadata version; remaining bytes are not "
                 "decoded\n";
    printBytes(0, data.size(), "file");
    return;
  }
  if (metadata.size != data.size()) {
    std::cerr << "Metadata size does not match the file length; remaining "
                 "sections are not decoded\n";
    printBytes(0, data.size(), "file");
    return;
  }

  const uint64_t instruction_bytes =
      uint64_t{metadata.num_instruction} * sizeof(InsnMeta);
  const uint64_t operand_bytes =
      uint64_t{metadata.num_operands} * sizeof(IMetaData::Operand);
  const uint64_t expression_bytes =
      uint64_t{metadata.num_expressions} * sizeof(IMetaData::Expression);
  const uint64_t instruction_end =
      uint64_t{metadata.off_instruction} + instruction_bytes;
  const uint64_t operand_end = uint64_t{metadata.off_operands} + operand_bytes;
  const uint64_t expression_end =
      uint64_t{metadata.off_expressions} + expression_bytes;

  const bool sections_valid = metadata.off_instruction == sizeof(IMetaData) &&
                              instruction_end == metadata.off_operands &&
                              operand_end == metadata.off_expressions &&
                              expression_end == metadata.off_handlers &&
                              metadata.off_handlers <= data.size();
  if (!sections_valid) {
    std::cerr << "Invalid or overlapping metadata section offsets/counts; "
                 "remaining bytes are not decoded\n";
    printBytes(0, data.size(), "file");
    return;
  }

  std::set<uint32_t> referenced_offsets;
  std::cout << "\nInstructions (" << metadata.num_instruction << "):\n";
  for (uint32_t i = 0; i < metadata.num_instruction; ++i) {
    InsnMeta instruction{};
    const uint64_t offset =
        uint64_t{metadata.off_instruction} + uint64_t{i} * sizeof(instruction);
    readAt(offset, instruction);
    if (instruction.handler != UINT32_MAX)
      referenced_offsets.insert(instruction.handler);
    std::cout << "  [" << i << "] rva=0x" << std::hex << instruction.rva
              << " handler-offset=";
    if (instruction.handler == UINT32_MAX)
      std::cout << "none";
    else
      std::cout << "0x" << instruction.handler;
    std::cout << std::dec << '\n';
  }

  std::cout << "\nOperands (" << metadata.num_operands << "):\n";
  for (uint32_t i = 0; i < metadata.num_operands; ++i) {
    IMetaData::Operand operand{};
    const uint64_t offset =
        uint64_t{metadata.off_operands} + uint64_t{i} * sizeof(operand);
    readAt(offset, operand);
    std::cout << "  [" << i << "] type=" << operandTypeName(operand.type)
              << " (" << static_cast<unsigned>(operand.type) << "), action=";
    switch (static_cast<remill::Operand::Action>(operand.action)) {
    case remill::Operand::kActionInvalid:
      std::cout << "invalid";
      break;
    case remill::Operand::kActionRead:
      std::cout << "read";
      break;
    case remill::Operand::kActionWrite:
      std::cout << "write";
      break;
    default:
      std::cout << "unknown(" << static_cast<unsigned>(operand.action) << ')';
      break;
    }
    std::cout << ", size=" << operand.size << " bits";
    switch (static_cast<remill::Operand::Type>(operand.type)) {
    case remill::Operand::kTypeRegister:
      std::cout << ", register={offset=" << operand.value.reg.offset
                << ", size=" << operand.value.reg.size << '}';
      break;
    case remill::Operand::kTypeShiftRegister: {
      const auto &value = operand.value.shift_register;
      std::cout << ", shift-register={register={offset=" << value.reg.offset
                << ", size=" << value.reg.size
                << "}, shift-size=" << static_cast<unsigned>(value.shift_size)
                << ", extract-size="
                << static_cast<unsigned>(value.extract_size)
                << ", shift-first=" << static_cast<unsigned>(value.shift_first)
                << ", can-shift-op-size="
                << static_cast<unsigned>(value.can_shift_op_size)
                << ", shift-op=" << static_cast<unsigned>(value.shift_op)
                << ", extend-op=" << static_cast<unsigned>(value.extend_op)
                << '}';
      break;
    }
    case remill::Operand::kTypeImmediate:
      std::cout << ", immediate={value=0x" << std::hex
                << operand.value.immediate.value << std::dec << ", signed="
                << static_cast<unsigned>(operand.value.immediate.is_signed)
                << '}';
      break;
    case remill::Operand::kTypeAddress: {
      const auto &value = operand.value.address;
      std::cout << ", address={segment-register={offset="
                << value.regs[0].offset << ", size=" << value.regs[0].size
                << "}, base-register={offset=" << value.regs[1].offset
                << ", size=" << value.regs[1].size
                << "}, index-register={offset=" << value.regs[2].offset
                << ", size=" << value.regs[2].size << "}, scale=" << value.scale
                << ", displacement=" << value.displacement << ", address-size="
                << static_cast<unsigned>(value.address_size)
                << ", kind=" << static_cast<unsigned>(value.kind) << '}';
      break;
    }
    case remill::Operand::kTypeExpression:
    case remill::Operand::kTypeRegisterExpression:
    case remill::Operand::kTypeImmediateExpression:
    case remill::Operand::kTypeAddressExpression: {
      const auto &value = operand.value.expression;
      std::cout << ", expression={kind=" << expressionKindName(value.kind)
                << " (" << static_cast<unsigned>(value.kind)
                << "), opcode=" << static_cast<unsigned>(value.opcode)
                << ", width=" << static_cast<unsigned>(value.width)
                << ", reserved=" << static_cast<unsigned>(value.reserved);
      switch (static_cast<IMetaData::Expression::Kind>(value.kind)) {
      case IMetaData::Expression::kOperation:
        std::cout << ", operation={lhs=" << value.payload.operation.lhs
                  << ", rhs=" << value.payload.operation.rhs << '}';
        break;
      case IMetaData::Expression::kRegister:
        std::cout << ", register={offset=" << value.payload.reg.offset
                  << ", size=" << value.payload.reg.size << '}';
        break;
      case IMetaData::Expression::kConstant:
        std::cout << ", constant={low=0x" << std::hex
                  << value.payload.constant.low << ", high=0x"
                  << value.payload.constant.high << std::dec << '}';
        break;
      case IMetaData::Expression::kInvalid:
      default:
        std::cout << ", payload-bytes=[";
        for (size_t j = 0; j < sizeof(value.payload); ++j)
          std::cout << (j ? " " : "") << std::hex << std::setw(2)
                    << std::setfill('0')
                    << static_cast<unsigned>(reinterpret_cast<const uint8_t *>(
                           &value.payload)[j]);
        std::cout << std::dec << std::setfill(' ') << ']';
        break;
      }
      std::cout << '}';
      break;
    }
    case remill::Operand::kTypeInvalid:
      break;
    default:
      std::cout << ", payload-bytes=[";
      for (size_t j = 0; j < sizeof(operand.value); ++j)
        std::cout << (j ? " " : "") << std::hex << std::setw(2)
                  << std::setfill('0')
                  << static_cast<unsigned>(
                         reinterpret_cast<const uint8_t *>(&operand.value)[j]);
      std::cout << std::dec << std::setfill(' ') << ']';
      break;
    }
    std::cout << '\n';
  }

  std::cout << "\nExpressions (" << metadata.num_expressions << "):\n";
  for (uint32_t i = 0; i < metadata.num_expressions; ++i) {
    IMetaData::Expression expression{};
    const uint64_t offset =
        uint64_t{metadata.off_expressions} + uint64_t{i} * sizeof(expression);
    readAt(offset, expression);
    std::cout << "  [" << i << "] kind=" << expressionKindName(expression.kind)
              << " (" << static_cast<unsigned>(expression.kind)
              << "), opcode=" << static_cast<unsigned>(expression.opcode)
              << ", width=" << static_cast<unsigned>(expression.width)
              << ", reserved=" << static_cast<unsigned>(expression.reserved);
    switch (static_cast<IMetaData::Expression::Kind>(expression.kind)) {
    case IMetaData::Expression::kOperation:
      std::cout << ", operation={lhs=" << expression.payload.operation.lhs
                << ", rhs=" << expression.payload.operation.rhs << '}';
      break;
    case IMetaData::Expression::kRegister:
      std::cout << ", register={offset=" << expression.payload.reg.offset
                << ", size=" << expression.payload.reg.size << '}';
      break;
    case IMetaData::Expression::kConstant:
      std::cout << ", constant={low=0x" << std::hex
                << expression.payload.constant.low << ", high=0x"
                << expression.payload.constant.high << std::dec << '}';
      break;
    case IMetaData::Expression::kInvalid:
    default:
      std::cout << ", payload-bytes=[";
      for (size_t j = 0; j < sizeof(expression.payload); ++j)
        std::cout << (j ? " " : "") << std::hex << std::setw(2)
                  << std::setfill('0')
                  << static_cast<unsigned>(reinterpret_cast<const uint8_t *>(
                         &expression.payload)[j]);
      std::cout << std::dec << std::setfill(' ') << ']';
      break;
    }
    std::cout << '\n';
  }

  std::cout << "\nHandlers (" << metadata.num_handlers << "):\n";
  constexpr uint64_t fixed_size = offsetof(IMetaData::Handler, opcode);
  using HandlerType = OpcodeHandler<uint8_t>::Type;
  auto recordSize = [&](const IMetaData::Handler &handler) {
    return fixed_size + handler.oplen +
           (handler.type == HandlerType::OHT_Remill
                ? uint64_t{handler.oprnum} * sizeof(uint16_t)
                : 0);
  };

  bool aotimpl = metadata.flags & IMetaData::kFlag_HasWX;
  std::set<uint32_t> computed_offsets;
  uint64_t handler_offset = metadata.off_handlers;
  for (uint32_t i = 0; i < metadata.num_handlers; ++i) {
    IMetaData::Handler handler{};
    if (handler_offset > data.size() ||
        fixed_size > data.size() - handler_offset) {
      std::cerr << "Truncated handler record " << i << '\n';
      return;
    }
    std::memcpy(&handler, data.data() + handler_offset, fixed_size);
    uint64_t record_size = recordSize(handler);
    if (handler.type > HandlerType::OHT_PrebuiltMapped ||
        record_size > data.size() - handler_offset) {
      std::cerr << "Invalid or truncated handler record " << i << '\n';
      return;
    }

    computed_offsets.insert(
        static_cast<uint32_t>(handler_offset - metadata.off_handlers));
    const auto *record = data.data() + handler_offset;
    std::cout << "  [" << i << "] section-offset=0x" << std::hex
              << handler_offset - metadata.off_handlers << std::dec
              << (handler.type == HandlerType::OHT_Dynamic
                      ? ", implementation-size="
                      : ", implementation-index=")
              << handler.impl << ", type=" << handlerTypeName(handler.type)
              << " (" << static_cast<unsigned>(handler.type)
              << "), opcode-length=" << static_cast<unsigned>(handler.oplen)
              << ", operand-count=" << static_cast<unsigned>(handler.oprnum)
              << ", opcode=";
    for (uint8_t j = 0; j < handler.oplen; ++j)
      std::cout << (j ? " " : "") << std::hex << std::setw(2)
                << std::setfill('0')
                << static_cast<unsigned>(record[fixed_size + j]);
    std::cout << std::dec << std::setfill(' ');
    if (handler.type == HandlerType::OHT_Remill) {
      std::cout << ", operands=[";
      for (uint8_t j = 0; j < handler.oprnum; ++j) {
        uint16_t value = 0;
        std::memcpy(&value,
                    record + fixed_size + handler.oplen +
                        uint64_t{j} * sizeof(value),
                    sizeof(value));
        std::cout << (j ? ", " : "") << "index=" << (value & 0x7fff)
                  << ", pointer=" << ((value & 0x8000) != 0);
      }
      std::cout << ']';
    }
    std::cout << '\n';

    if (!aotimpl) {
      handler_offset += record_size;
      continue;
    }

    // skip the aot implementation (impl holds its size), 4-byte aligned
    if (handler.type == HandlerType::OHT_Dynamic)
      record_size = ((record_size + 3u) & ~uint64_t{3}) + handler.impl;
    record_size = (record_size + 3u) & ~uint64_t{3};
    handler_offset += record_size;
  }

  // verify the calculated handler offsets against the instruction metadata
  size_t mismatches = 0;
  for (auto offset : referenced_offsets) {
    if (computed_offsets.contains(offset))
      continue;
    ++mismatches;
    std::cerr << "Instruction references handler offset 0x" << std::hex
              << offset << std::dec
              << " which does not match any calculated handler offset\n";
  }
  if (mismatches)
    std::cerr << "Handler offset verification failed: " << mismatches
              << " mismatch(es)\n";
}

} // namespace aether

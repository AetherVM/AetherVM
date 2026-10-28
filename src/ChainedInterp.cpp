// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "ChainedInterp.h"
#include "CPUState.h"

#include <Platform.h>
#include <Utils.h>

#include <algorithm>

namespace aether {

ChainedInterp chainedInterp;

RuntimeMetaData::RuntimeMetaData(std::string_view path,
                                 std::string_view aevmpath, uint64_t rtbase,
                                 uint64_t rtend)
    : path(path), rtbase(rtbase), rtend(rtend) {
  metadata =
      reinterpret_cast<const IMetaData *>(map_file_readonly(aevmpath, &mdsize));
  insns = metadata->insns();

  nexticache.resize(metadata->num_instruction);
  // Initialize the next instruction cache with sequential indices
  for (size_t i = 0; i < nexticache.size(); ++i)
    nexticache[i] = i + 1;
}

RuntimeMetaData::~RuntimeMetaData() {
  // Unmap the metadata file
  unmap_file(reinterpret_cast<uintptr_t>(metadata), mdsize);
}

const IMetaData::InsnMeta *RuntimeMetaData::find(uint64_t addr) const {
  IMetaData::InsnMeta key;
  key.rva = (uint32_t)(addr - rtbase);
  auto found = binary_search(insns, metadata->num_instruction, key);
  return found->rva == key.rva ? found : nullptr;
}

std::string ChainedInterp::add(std::string_view path, uint64_t rtbase,
                               uint64_t rtend) {
  auto aevm =
      (fs::path(cacheRoot) / fs::path(path).filename()).string() + ".aevm";
  if (!fs::exists(aevm))
    return std::format("{} does not exist", aevm);

  runtimes.emplace_back(path, aevm, rtbase, rtend);

  // Sort the runtimes vector in place by their base address (rtbase)
  std::sort(runtimes.begin(), runtimes.end());
  return "";
}

void ChainedInterp::reset() { runtimes.clear(); }

const RuntimeMetaData *
ChainedInterp::find(uint64_t addr, const IMetaData::InsnMeta **insn) const {
  RuntimeMetaData key;
  key.rtbase = addr;
  auto rtfound = binary_search(runtimes.data(), runtimes.size(), key);
  if (rtfound == runtimes.data() + runtimes.size())
    rtfound = &runtimes.back();
  if (addr > rtfound->rtend)
    return nullptr;
  if (insn)
    *insn = rtfound->find(addr);
  return rtfound;
}

int ChainedInterp::main(uint64_t entry, int argc, const char *argv[]) {
  const IMetaData::InsnMeta *insn = nullptr;
  auto runtime = find(entry, &insn);
  if (!runtime)
    return -1; // Entry not found in any runtime
  if (!insn)
    return -1; // Instruction not found in the runtime

  // Set up CPU registers for program arguments based on architecture and OS for
  // guest main function
#if AETHER_ARCH_ARM64
  auto result = []() { return CPU.getRegisterAArch64(Register::X0)->u4; };
  CPU.setRegisterAArch64(Register::X0, {.u8 = (uint64_t)argc});
  CPU.setRegisterAArch64(Register::X1, {.ptr = argv});
#else
  auto result = []() { return CPU.getRegisterX86(Register::RAX)->u4; };
#if AETHER_OS_WINDOWS
  CPU.setRegisterX86(Register::RCX, {.u8 = (uint64_t)argc});
  CPU.setRegisterX86(Register::RDX, {.ptr = argv});
#else
  CPU.setRegisterX86(Register::RDI, {.u8 = (uint64_t)argc});
  CPU.setRegisterX86(Register::RSI, {.ptr = argv});
#endif
#endif
  vmEntry(runtime, insn);
  return result();
}

void ChainedInterp::vmEntry(const RuntimeMetaData *rt,
                            const IMetaData::InsnMeta *insn) {}

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <AetherVM.h>
#include <Memory.h>
#include <Utils.h>

#include <mutex>

namespace aether {

struct BinaryEngineImpl {
  std::mutex mutex;
  std::vector<EventCallback> eventCallbacks;
  GuestMemory guestMemory;
};

// shortcuts for engine implementation
#define engine ((BinaryEngineImpl *)m_impl)
#define lock_on() std::lock_guard<std::mutex> _lock_(engine->mutex)
#define callbacks (engine->eventCallbacks)
#define memory (engine->guestMemory)

BinaryEngine::BinaryEngine(const Machine *mach) : m_machine(mach) {
  m_impl = new BinaryEngineImpl;
}

BinaryEngine::BinaryEngine(const Binary *bin) : m_binary(bin) {
  // use the image base from bin so that we can directly map section memory
  memory.baseGuest = bin->imageBase();
  // calculate the total page size of all the sections
  size_t size = 0;
  for (auto &[addr, sect] : bin->sections())
    size += sect.size;
  size = align_up(size, page_size());

  // map all the sections
  memory.commit(bin->imageBase(), size, true, true);
  for (auto &[addr, sect] : bin->sections()) {
    if (!sect.size)
      continue;
    auto sectbuff = reinterpret_cast<const uint8_t *>(bin->addrBuff(sect.addr));
    writeMemory(sect.addr, {sectbuff, sect.size});
  }
}

BinaryEngine::~BinaryEngine() { delete engine; }

bool BinaryEngine::execute(std::span<const uint8_t> raw) {
  auto vmaddr = mapMemory(raw.size());
  if (!vmaddr)
    return false;

  writeMemory(vmaddr, raw);
  return execute(vmaddr);
}

bool BinaryEngine::execute(addr_t target) {
  if (!memory.valid(target, 1))
    return false;

  RegisterValue pc{.b8 = target};
  setRegister(Register::PC, pc);
  return false;
}

bool BinaryEngine::runMain() { return false; }

const void *BinaryEngine::makeExecutable(std::span<const uint8_t> raw) {
  return nullptr;
}

const RegisterValue *BinaryEngine::getRegister(Register reg) { return nullptr; }

void BinaryEngine::setRegister(Register reg, RegisterValue val) {}

addr_t BinaryEngine::mapMemory(size_t size) {
  auto vmaddr = memory.guestAvailable();
  size = align_up(size, page_size());
  return memory.commit(vmaddr, size, true, true) ? vmaddr : 0;
}

std::vector<uint8_t> BinaryEngine::readMemory(addr_t addr, size_t size) {
  std::vector<uint8_t> buff;
  if (memory.valid(addr, size)) {
    buff.resize(size);
    std::memcpy(buff.data(), reinterpret_cast<void *>(memory.host(addr)), size);
  }
  return buff;
}

bool BinaryEngine::writeMemory(addr_t addr, std::span<const uint8_t> buff) {
  if (!memory.valid(addr, buff.size()))
    return false;
  std::memcpy(reinterpret_cast<void *>(memory.host(addr)), buff.data(),
              buff.size());
  return true;
}

int BinaryEngine::registerCallback(EventCallback callback) {
  callbacks.push_back(callback);
  return (int)callbacks.size();
}

} // namespace aether

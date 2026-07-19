// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <AetherVM.h>

#include <mutex>

namespace aether {

struct BinaryEngineImpl {
  std::mutex mutex;
  std::vector<EventCallback> eventCallbacks;
};

BinaryEngine::BinaryEngine(const Machine *mach) : m_machine(mach) {}

BinaryEngine::BinaryEngine(const Binary *bin) : m_binary(bin) {}

BinaryEngine::~BinaryEngine() {}

bool BinaryEngine::execute(std::span<uint8_t> raw) { return false; }

bool BinaryEngine::execute(addr_t target) { return false; }

bool BinaryEngine::runMain() { return false; }

const void *BinaryEngine::makeExecutable(std::span<uint8_t> raw) {
  return nullptr;
}

const RegisterValue *BinaryEngine::getRegister(Register reg) { return nullptr; }

void BinaryEngine::setRegister(Register reg, RegisterValue val) {}

int BinaryEngine::registerCallback(EventCallback callback) { return -1; }

} // namespace aether

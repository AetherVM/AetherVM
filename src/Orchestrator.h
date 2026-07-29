// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include <AetherBinary.h>

namespace aether {

class Orchestrator {
public:
  static Orchestrator *inst() {
    static Orchestrator single;
    return &single;
  }

  void encode(const Binary *bin, addr_t addend);

private:
  Orchestrator() {}
  ~Orchestrator() {}
};

} // namespace aether

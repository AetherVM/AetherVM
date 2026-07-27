// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Handler.h>
#include <Utils.h>
#include <algorithm>

namespace aether {

std::vector<Handler> Handler::aarch64;
std::vector<Handler> Handler::x86;

static void load_handler(std::vector<Handler> &handlers, const HandlerRaw *raw,
                         size_t num) {
  handlers.resize(num);
  std::transform(raw, raw + num, handlers.begin(), [](const HandlerRaw &r) {
    return Handler{hash_value(r.name), *r.handler};
  });
  std::sort(handlers.begin(), handlers.end());
}

void Handler::loadAArch64() {
  load_handler(aarch64, &HandlerAArch64[0], HandlerAArch64Num);
}

void Handler::loadX86() { load_handler(x86, &HandlerX86[0], HandlerX86Num); }

} // namespace aether

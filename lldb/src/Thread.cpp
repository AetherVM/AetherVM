// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "Thread.h"

namespace lldb_private {

AetherThread::AetherThread(NativeProcessProtocol &process, lldb::tid_t tid,
                           uintptr_t *pcptr, bool arm64)
    : NativeThreadProtocol(process, tid), m_pcptr(pcptr),
      m_reg_context(arm64 ? reinterpret_cast<NativeRegisterContext *>(
                                new AetherRegisterContextARM64(*this))
                          : reinterpret_cast<NativeRegisterContext *>(
                                new AetherRegisterContextX64(*this))) {}

} // namespace lldb_private

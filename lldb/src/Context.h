// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "lldb/Host/common/NativeRegisterContext.h"

namespace lldb_private {

class VMRegisterContext : public NativeRegisterContext {
public:
  VMRegisterContext(NativeThreadProtocol &thread)
      : NativeRegisterContext(thread) {}

  uint32_t GetRegisterCount() const override { abort(); }

  const RegisterInfo *
  GetRegisterInfoAtIndex(uint32_t reg_index) const override {
    abort();
    return nullptr;
  }

  Status ReadRegister(const RegisterInfo *reg_info,
                      RegisterValue &reg_value) override {
    abort();
    return Status();
  }

  Status WriteRegister(const RegisterInfo *reg_info,
                       const RegisterValue &reg_value) override {
    abort();
    return Status();
  }

  Status ReadAllRegisterValues(lldb::WritableDataBufferSP &data_sp) override {
    abort();
    return Status();
  }
  Status WriteAllRegisterValues(const lldb::DataBufferSP &data_sp) override {
    abort();
    return Status();
  }
};

} // namespace lldb_private

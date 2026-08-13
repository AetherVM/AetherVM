// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "lldb/Host/common/NativeRegisterContext.h"

namespace aether {
class BinaryEngine;
}

namespace lldb_private {

extern aether::BinaryEngine *Engine;

class AetherRegisterContextARM64 : public NativeRegisterContext {
public:
  AetherRegisterContextARM64(NativeThreadProtocol &thread)
      : NativeRegisterContext(thread) {}

  uint32_t GetRegisterCount() const override;

  const RegisterInfo *GetRegisterInfoAtIndex(uint32_t reg_index) const override;

  Status ReadRegister(const RegisterInfo *reg_info,
                      RegisterValue &reg_value) override;

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

  uint32_t GetUserRegisterCount() const override { return 0; }

  uint32_t GetRegisterSetCount() const override { return 0; }

  const RegisterSet *GetRegisterSet(uint32_t set_index) const override {
    abort();
  }
};

class AetherRegisterContextX64 : public NativeRegisterContext {
public:
  AetherRegisterContextX64(NativeThreadProtocol &thread)
      : NativeRegisterContext(thread) {}

  uint32_t GetRegisterCount() const override;

  const RegisterInfo *GetRegisterInfoAtIndex(uint32_t reg_index) const override;

  Status ReadRegister(const RegisterInfo *reg_info,
                      RegisterValue &reg_value) override;

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

  uint32_t GetUserRegisterCount() const override { return 0; }

  uint32_t GetRegisterSetCount() const override { return 0; }

  const RegisterSet *GetRegisterSet(uint32_t set_index) const override {
    abort();
  }
};

} // namespace lldb_private

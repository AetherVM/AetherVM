// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "lldb/Host/common/NativeRegisterContext.h"

namespace lldb_private {

class AetherRegisterContextARM64 : public NativeRegisterContext {
public:
  AetherRegisterContextARM64(NativeThreadProtocol &thread)
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

  uint32_t GetUserRegisterCount() const override { abort(); }

  uint32_t GetRegisterSetCount() const override { abort(); }

  const RegisterSet *GetRegisterSet(uint32_t set_index) const override {
    abort();
  }
};

class AetherRegisterContextX64 : public NativeRegisterContext {
public:
  AetherRegisterContextX64(NativeThreadProtocol &thread)
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

  uint32_t GetUserRegisterCount() const override { abort(); }

  uint32_t GetRegisterSetCount() const override { abort(); }

  const RegisterSet *GetRegisterSet(uint32_t set_index) const override {
    abort();
  }
};

} // namespace lldb_private

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#pragma once

#include "lldb/Host/common/NativeRegisterContext.h"

#include <AetherVM.h>

namespace lldb_private {

extern aether::BinaryEngine *Engine;

class AetherRegisterContext : public NativeRegisterContext {
public:
  AetherRegisterContext(NativeThreadProtocol &thread)
      : NativeRegisterContext(thread) {}

  uint32_t GetUserRegisterCount() const override { return GetRegisterCount(); }

  uint32_t GetRegisterSetCount() const override { return 0; }

  Status ReadAllRegisterValues(lldb::WritableDataBufferSP &data_sp) override {
    abort();
    return Status();
  }

  Status WriteAllRegisterValues(const lldb::DataBufferSP &data_sp) override {
    abort();
    return Status();
  }

  const RegisterSet *GetRegisterSet(uint32_t set_index) const override {
    abort();
  }

  static void InitOffsets();

protected:
  void *GetCPU() const;

  Status DoReadRegister(const RegisterInfo *reg_info,
                        const RegisterInfo *reg_infos,
                        const aether::Register *registers,
                        RegisterValue &reg_value);
};

class AetherRegisterContextARM64 : public AetherRegisterContext {
public:
  AetherRegisterContextARM64(NativeThreadProtocol &thread)
      : AetherRegisterContext(thread) {}

  uint32_t GetRegisterCount() const override;

  const RegisterInfo *GetRegisterInfoAtIndex(uint32_t reg_index) const override;

  Status ReadRegister(const RegisterInfo *reg_info,
                      RegisterValue &reg_value) override;

  Status WriteRegister(const RegisterInfo *reg_info,
                       const RegisterValue &reg_value) override {
    abort();
    return Status();
  }
};

class AetherRegisterContextX64 : public AetherRegisterContext {
public:
  AetherRegisterContextX64(NativeThreadProtocol &thread)
      : AetherRegisterContext(thread) {}

  uint32_t GetRegisterCount() const override;

  const RegisterInfo *GetRegisterInfoAtIndex(uint32_t reg_index) const override;

  Status ReadRegister(const RegisterInfo *reg_info,
                      RegisterValue &reg_value) override;

  Status WriteRegister(const RegisterInfo *reg_info,
                       const RegisterValue &reg_value) override {
    abort();
    return Status();
  }
};

} // namespace lldb_private

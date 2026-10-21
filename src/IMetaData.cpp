// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "IMetaData.h"

#include <type_traits>

namespace aether {

static_assert(sizeof(IMetaData) == 48);
static_assert(sizeof(IMetaData::Operand::Register) == 4);
static_assert(sizeof(IMetaData::Operand::ShiftRegister) == 10);
static_assert(sizeof(IMetaData::Operand::Immediate) == 16);
static_assert(sizeof(IMetaData::Operand::Address) == 40);
static_assert(sizeof(IMetaData::Operand) == 48);
static_assert(sizeof(IMetaData::Expression::Operation) == 8);
static_assert(sizeof(IMetaData::Expression::Register) == 4);
static_assert(sizeof(IMetaData::Expression::Constant) == 8);
static_assert(sizeof(IMetaData::Expression) == 12);
static_assert(std::is_trivially_copyable_v<IMetaData>);
static_assert(std::is_trivially_copyable_v<IMetaData::Operand>);
static_assert(std::is_trivially_copyable_v<IMetaData::Expression>);

} // namespace aether

# AetherVM - Lift. Instrument. Emulate. Recover.
# Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
# SPDX-License-Identifier: Apache License, Version 2.0
# See LICENSE file in the root directory for full license text.

if(POLICY CMP0160)
  cmake_policy(SET CMP0160 OLD)
endif()

if(CMAKE_HOST_WIN32)
  set(HOST_EXECUTABLE_SUFFIX ".exe")
else()
  set(HOST_EXECUTABLE_SUFFIX "")
endif()

if(NOT TARGET llvm-link)
  # to let get_target_property(LLVMLINK_PATH llvm-link LOCATION) work in remill
  add_executable(llvm-link IMPORTED GLOBAL)
  set_target_properties(llvm-link PROPERTIES
    IMPORTED_LOCATION "${ICPP_INSTALL_DIR}/bin/llvm-link${HOST_EXECUTABLE_SUFFIX}"
    LOCATION "${ICPP_INSTALL_DIR}/bin/llvm-link${HOST_EXECUTABLE_SUFFIX}"
  )
endif()

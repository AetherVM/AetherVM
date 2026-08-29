# AetherVM - Lift. Instrument. Emulate. Recover.
# Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
# SPDX-License-Identifier: Apache License, Version 2.0
# See LICENSE file in the root directory for full license text.

# clang compiler from icpp package
set(CMAKE_C_COMPILER "${ICPP_INSTALL_DIR}/bin/clang")
set(CMAKE_CXX_COMPILER "${ICPP_INSTALL_DIR}/bin/clang++")

# apply the icpp's c++ runtime
set(ICPP_CXX_LDFLAGS " -nostdlib++ ${ICPP_INSTALL_DIR}/lib/libunwind.so.1 ${ICPP_INSTALL_DIR}/lib/libc++abi.so.1 ${ICPP_INSTALL_DIR}/lib/libc++.so.1 ${LLVM_BUILD_DIR}/lib/libLLVMSupport.a")
string(APPEND CMAKE_CXX_FLAGS " -nostdinc++ -nostdlib++ -I${ICPP_INSTALL_DIR}/include/c++/v1")
string(APPEND CMAKE_EXE_LINKER_FLAGS ${ICPP_CXX_LDFLAGS})
string(APPEND CMAKE_SHARED_LINKER_FLAGS ${ICPP_CXX_LDFLAGS})

# enable -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT TARGET llvm-link)
  # to let get_target_property(LLVMLINK_PATH llvm-link LOCATION) work in remill
  add_executable(llvm-link IMPORTED GLOBAL)
  set_target_properties(llvm-link PROPERTIES
    IMPORTED_LOCATION "${ICPP_INSTALL_DIR}/bin/llvm-link"
    LOCATION "${ICPP_INSTALL_DIR}/bin/llvm-link"
  )
endif()

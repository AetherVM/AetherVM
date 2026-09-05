# AetherVM - Lift. Instrument. Emulate. Recover.
# Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
# SPDX-License-Identifier: Apache License, Version 2.0
# See LICENSE file in the root directory for full license text.

# clang compiler from icpp package
if(WIN32)
  if(POLICY CMP0160)
    cmake_policy(SET CMP0160 OLD)
  endif()
  if(NOT DEFINED CMAKE_EXECUTABLE_SUFFIX)
    set(CMAKE_EXECUTABLE_SUFFIX ".exe")
  endif()

  set(CMAKE_C_COMPILER "${ICPP_INSTALL_DIR}/bin/clang-cl${CMAKE_EXECUTABLE_SUFFIX}")
  set(CMAKE_CXX_COMPILER "${CMAKE_C_COMPILER}")
else()
  set(CMAKE_C_COMPILER "${ICPP_INSTALL_DIR}/bin/clang")
  set(CMAKE_CXX_COMPILER "${CMAKE_C_COMPILER}++")
endif()

# apply the icpp's c++ runtime
if(WIN32)
  set(ICPP_CXX_LDFLAGS " /libpath:${ICPP_INSTALL_DIR}/lib /nodefaultlib:msvcprt.lib c++.lib cxxabi_msvc.lib clang_rt.builtins.lib /FORCE:MULTIPLE")
  string(APPEND CMAKE_CXX_FLAGS " /clang:-nostdinc++ /clang:-nostdlib++ -I${ICPP_INSTALL_DIR}/include/c++/v1 -D_LIBCPP_NO_AUTO_LINK")
else()
  set(ICPP_CXX_LDFLAGS " -nostdlib++ ${ICPP_INSTALL_DIR}/lib/libunwind.so.1 ${ICPP_INSTALL_DIR}/lib/libc++abi.so.1 ${ICPP_INSTALL_DIR}/lib/libc++.so.1 ${LLVM_BUILD_DIR}/lib/libLLVMSupport.a")
  string(APPEND CMAKE_CXX_FLAGS " -nostdinc++ -nostdlib++ -I${ICPP_INSTALL_DIR}/include/c++/v1")
endif()
string(APPEND CMAKE_EXE_LINKER_FLAGS ${ICPP_CXX_LDFLAGS})
string(APPEND CMAKE_SHARED_LINKER_FLAGS ${ICPP_CXX_LDFLAGS})

# enable -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT BUILDING_AETHERDBG AND NOT TARGET llvm-link)
  # to let get_target_property(LLVMLINK_PATH llvm-link LOCATION) work in remill
  add_executable(llvm-link IMPORTED GLOBAL)
  set_target_properties(llvm-link PROPERTIES
    IMPORTED_LOCATION "${ICPP_INSTALL_DIR}/bin/llvm-link${CMAKE_EXECUTABLE_SUFFIX}"
    LOCATION "${ICPP_INSTALL_DIR}/bin/llvm-link${CMAKE_EXECUTABLE_SUFFIX}"
  )
endif()

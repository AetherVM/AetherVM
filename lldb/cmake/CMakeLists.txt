# AetherVM - Lift. Instrument. Emulate. Recover.
# Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
# SPDX-License-Identifier: Apache License, Version 2.0
# See LICENSE file in the root directory for full license text.

# cmake
cmake_minimum_required(VERSION 3.20)

# Default to a Release config. Required before project() because Windows defaults to Debug
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

project(AetherLLDB)

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

# enable only the AArch64 target to minimize build time and size, we don't need any targets 
# in AetherVM actually
set(LLVM_TARGETS_TO_BUILD "AArch64" CACHE STRING "" FORCE)

# disable unused components
set(LLVM_ENABLE_ZLIB "OFF" CACHE STRING "" FORCE)
set(LLVM_ENABLE_ZSTD "OFF" CACHE STRING "" FORCE)
set(LLVM_INCLUDE_RUNTIMES OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_UTILS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LLVM_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLVM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(LLVM_ENABLE_PROJECTS "lldb" CACHE STRING "" FORCE)
set(LLDB_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
set(LLDB_ENABLE_LUA OFF CACHE BOOL "" FORCE)
set(LLDB_ENABLE_CURSES OFF CACHE BOOL "" FORCE)
set(LLDB_ENABLE_LIBXML2 OFF CACHE BOOL "" FORCE)
set(LLDB_ENABLE_LZMA OFF CACHE BOOL "" FORCE)
add_subdirectory(${LLVM_PROJECT_ROOT}/llvm
                 ${CMAKE_CURRENT_BINARY_DIR}/lldb)

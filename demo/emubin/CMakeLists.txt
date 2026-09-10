# AetherVM - Lift. Instrument. Emulate. Recover.
# Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
# SPDX-License-Identifier: Apache License, Version 2.0
# See LICENSE file in the root directory for full license text.

cmake_minimum_required(VERSION 3.20)
project(emubin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(AetherBinary CONFIG REQUIRED)
find_package(AetherVM CONFIG REQUIRED)

add_executable(emubin emubin.cpp)
target_link_libraries(emubin PRIVATE 
  AetherBinary::AetherBinary
  AetherVM::AetherVM
)

if(UNIX)
  target_link_options(emubin PRIVATE -Wl,-rpath,${ICPP_INSTALL_DIR}/lib)
endif()

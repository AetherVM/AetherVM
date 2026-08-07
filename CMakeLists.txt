# AetherVM - Lift. Instrument. Emulate. Recover.
# Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
# SPDX-License-Identifier: Apache License, Version 2.0
# See LICENSE file in the root directory for full license text.

cmake_minimum_required(VERSION 3.20)
project(AetherVM VERSION 0.1.0 LANGUAGES C CXX)

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(PROJECT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# find the dependent packages built in AetherBinary project
find_package(LLVM CONFIG REQUIRED)
find_package(AetherBinary CONFIG REQUIRED)
find_package(remill CONFIG REQUIRED)

configure_file(
  "${CMAKE_SOURCE_DIR}/include/Version.h.in"
  "${CMAKE_BINARY_DIR}/generated/Version.h"
  @ONLY
)

# generate handlers for aarch64 and x86_64
execute_process(COMMAND ${ICPP_PATH} 
  ${CMAKE_SOURCE_DIR}/tool/handler_generator.cc 
  ${CMAKE_SOURCE_DIR}/third/remill 
  ${CMAKE_BINARY_DIR}/generated
)

set(HEADER_FILES
  include/AetherVM.h
  include/Event.h
  include/Platform.h
  include/Register.h
  include/Utils.h
  ${CMAKE_BINARY_DIR}/generated/Version.h
)

set(SOURCE_FILES
  src/AArch64.cpp
  src/AArch64.h
  src/AetherVM.cpp
  src/BinaryEngine.cpp
  src/BinaryEngine.h 
  src/CPUState.cpp    
  src/CPUState.h 
  src/Event.cpp
  src/Handler.cpp
  src/Handler.h
  src/HandlerAArch64.cpp
  src/HandlerX86.cpp
  src/Intrinsic.cpp
  src/Lifter.cpp
  src/Lifter.h
  src/Memory.cpp
  src/Memory.h
  src/Orchestrator.cpp
  src/Orchestrator.h
  src/Platform.cpp
  src/Undefine.cpp
  src/Utils.cpp
  src/X86.cpp
  src/X86.h
  ${CMAKE_BINARY_DIR}/generated/HandlerAArch64.cpp
  ${CMAKE_BINARY_DIR}/generated/HandlerX86.cpp
)

add_library(AetherVM SHARED
  ${SOURCE_FILES}
  ${HEADER_FILES}
)

# link the LLVM shared library
if(MSVC)
  target_link_directories(AetherVM PRIVATE ${LLVM_LIBRARY_DIRS})
  target_link_libraries(AetherVM PRIVATE LLVM-22)
else()
  target_link_libraries(AetherVM PRIVATE LLVM)
endif()
  
target_link_libraries(AetherVM PRIVATE 
  AetherBinary::AetherBinary
  remill
)

target_include_directories(AetherVM PRIVATE 
  include
  lldb/src
  third/remill
  third/remill/include
  ${LLVM_INCLUDE_DIRS}
)

if(APPLE)
  set_target_properties(AetherVM PROPERTIES
    SKIP_BUILD_RPATH TRUE
    SKIP_INSTALL_RPATH TRUE
  )
  target_link_options(AetherVM PRIVATE
    -Wl,-rpath,@executable_path
    -Wl,-rpath,@executable_path/../lib
    -Wl,-rpath,@loader_path
    -Wl,-rpath,@loader_path/../lib
    -Wl,-install_name,@rpath/libAetherVM.dylib
  )
elseif(MSVC)
else()
endif()

target_include_directories(AetherVM PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
  $<INSTALL_INTERFACE:include>
)

if(MSVC)
  target_compile_options(AetherVM PRIVATE 
    # /EHsc enables standard synchronous C++ exception handling
    /EHsc)

  set(DISABLE_RTTI /GR-)
else()
  set(DISABLE_RTTI -fno-rtti)
endif()

target_compile_options(AetherVM PRIVATE
  -DAETHER_DLLIMPL=1

  ${DISABLE_RTTI}
)
target_compile_features(AetherVM PUBLIC cxx_std_20)

target_compile_definitions(AetherVM PRIVATE
  PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  PROJECT_VERSION_STRING="${PROJECT_VERSION_STRING}"
)

# Install rules
install(TARGETS AetherVM
  EXPORT AetherVMTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

install(EXPORT AetherVMTargets
  NAMESPACE AetherVM::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AetherVM
)

configure_package_config_file(
  "${CMAKE_SOURCE_DIR}/cmake/AetherVMConfig.cmake.in"
  "${CMAKE_BINARY_DIR}/generated/AetherVMConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AetherVM
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
  "${CMAKE_BINARY_DIR}/generated/AetherVMConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_BINARY_DIR}/generated/AetherVMConfig.cmake"
  "${CMAKE_BINARY_DIR}/generated/AetherVMConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AetherVM
)

install(FILES
  "${remill_DIR}/../../../share/remill/22/semantics/aarch64.bc"
  "${remill_DIR}/../../../share/remill/22/semantics/amd64_avx512.bc"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/bitcode
)

if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(AetherDbg_BUILD_TYPE "RelWithDebInfo")
else()
  set(AetherDbg_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()

install(FILES
  ${CMAKE_SOURCE_DIR}/lldb/build-${AetherDbg_BUILD_TYPE}/libAetherDbg${CMAKE_SHARED_LIBRARY_SUFFIX}
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  OPTIONAL
)

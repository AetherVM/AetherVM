# AetherVM - Lift. Instrument. Emulate. Recover.
# Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later */
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

# find the dependent LLVM package, which is built in the previous step
find_package(LLVM CONFIG REQUIRED)

configure_file(
  "${CMAKE_SOURCE_DIR}/include/Version.h.in"
  "${CMAKE_BINARY_DIR}/generated/Version.h"
  @ONLY
)

set(HEADER_FILES
  include/AetherVM.h
)

set(SOURCE_FILES
  src/AetherVM.cpp
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

target_include_directories(AetherVM PRIVATE 
  ${LLVM_INCLUDE_DIRS}
)

if(APPLE)
  set_target_properties(AetherVM PROPERTIES
    SKIP_BUILD_RPATH TRUE
    SKIP_INSTALL_RPATH TRUE
    MACOSX_RPATH OFF
  )
  target_link_options(AetherVM PRIVATE
    -Wl,-rpath,@executable_path
    -Wl,-rpath,@executable_path/../lib
  )
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

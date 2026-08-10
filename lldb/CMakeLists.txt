# AetherVM - Lift. Instrument. Emulate. Recover.
# Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
# SPDX-License-Identifier: Apache License, Version 2.0
# See LICENSE file in the root directory for full license text.

cmake_minimum_required(VERSION 3.20)
project(AetherDbg VERSION 0.1.0 LANGUAGES C CXX)

find_package(LLVM CONFIG REQUIRED)
find_package(AetherVM CONFIG REQUIRED)

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCE_FILES
  src/Context.cpp
  src/Context.h
  src/Debugger.cpp
  src/Debugger.h
  src/Manager.cpp
  src/Manager.h
  src/Process.cpp
  src/Process.h
  src/Thread.cpp
  src/Thread.h
)

add_library(AetherDbg SHARED
  ${SOURCE_FILES}
)

# link the LLVM shared library
if(MSVC)
  target_link_directories(AetherDbg PRIVATE ${LLVM_LIBRARY_DIRS})
  target_link_libraries(AetherDbg PRIVATE LLVM-22 LLVMSupport)
else()
  target_link_libraries(AetherDbg PRIVATE LLVM LLVMSupport)
endif()

target_include_directories(AetherDbg PRIVATE 
  ../include
  ../src
  ../third/remill
  ../third/remill/include
  build-lldb/lldb/tools/lldb/include
  ${LLVM_PROJECT_ROOT}/lldb/include
  ${LLVM_PROJECT_ROOT}/lldb/source
  ${LLVM_INCLUDE_DIRS}
)

target_link_directories(AetherDbg PRIVATE
  build-lldb/lldb/lib
)
target_link_libraries(AetherDbg PRIVATE
  lldbInitialization     
  lldbPluginProcessGDBRemote    
  lldbCore  
  lldbSymbol  
  lldbTarget      
  lldbBreakpoint  
  lldbDataFormatters    
  lldbValueObject    
  lldbHost    
  lldbVersion  
  lldbUtility
  lldbInterpreter
  lldbCommands
  lldbExpression

  AetherVM::AetherVM
)

if(APPLE)
  target_link_libraries(AetherDbg PRIVATE
    lldbHostMacOSXObjCXX
  )
  set_target_properties(AetherDbg PROPERTIES
    SKIP_BUILD_RPATH TRUE
    SKIP_INSTALL_RPATH TRUE
  )
  target_link_options(AetherDbg PRIVATE
    -Wl,-rpath,@executable_path
    -Wl,-rpath,@executable_path/../lib
    -Wl,-rpath,@loader_path
    -Wl,-rpath,@loader_path/../lib
    -Wl,-install_name,@rpath/libAetherDbg.dylib
    -Wl,-framework,Foundation  
    -Wl,-framework,CoreFoundation
    -Wl,-framework,CoreServices
    -Wl,-framework,Security
    -ledit
    -lcompression
    -lobjc
  )
elseif(MSVC)
else()
endif()

target_include_directories(AetherDbg PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
  $<INSTALL_INTERFACE:include>
)

if(MSVC)
  target_compile_options(AetherDbg PRIVATE 
    # /EHsc enables standard synchronous C++ exception handling
    /EHsc)

  set(DISABLE_RTTI /GR-)
else()
  set(DISABLE_RTTI -fno-rtti)
endif()

target_compile_options(AetherDbg PRIVATE
  -DAETHER_DLLIMPL=1

  ${DISABLE_RTTI}
)
target_compile_features(AetherDbg PUBLIC cxx_std_20)

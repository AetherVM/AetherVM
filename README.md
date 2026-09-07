# AetherVM - Lift. Instrument. Emulate. Recover.
**AetherVM** is an LLVM-native binary analysis and emulation platform built around instruction lifting to [LLVM IR](http://llvm.org/docs/LangRef.html). It unifies static, dynamic, and symbolic analysis through a common intermediate representation, enabling advanced instrumentation, runtime recovery, deobfuscation, and program understanding.

AetherVM is motivated by and partially derived from [McSema](https://github.com/lifting-bits/mcsema), [Remill](https://github.com/lifting-bits/remill), and [ICPP](https://github.com/vpand/icpp).

## Build
### Windows prepare
If you're building on Windows, firstly setup the MSVC environment, otherwise skip this:
```sh
# all the following steps must be done in "x64 Native Tools Command Prompt for VS"
# or
# run VS_ROOT/.../VC/Auxiliary/Build/vcvarsall.bat to initialize for 'x64'
# replace it to arm64 if you're on a Windows-ARM64 device.
vcvarsall x64
```
### ICPP Build
To build your own version, you need to have [ICPP](https://github.com/vpand/icpp), **Ninja**, **CMake** available in current terminal session. After a recursive clone of this repo, then build it in one go:
```sh
# build AetherVM
/path/to/icpp build.cc
# build AetherDbg if you'd like
/path/to/icpp lldb/build.cc
```
After aaa... while, the package should be at: build-Release/install.

## Issue
If you encounter any problems when using AetherVM, before opening an issue, please check the [Bug Report](https://github.com/AetherVM/AetherVM/blob/main/.github/ISSUE_TEMPLATE/bug_report.md) template, and provide as many details as you can. Only if we can reproduce the problem, we can then solve it.

## Contact
If you have any questions or thoughts, just feel free to email to me:
```
neoliu2011@gmail.com
```
Any feedback is welcome.

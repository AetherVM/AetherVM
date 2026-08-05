// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

// usage: icpp build.cc [Debug]

#include <icpp.hpp>

namespace fs = std::filesystem;

#if 1 // change to 0 to only print the command without executing it.
#define command(fmt, ...) std::system(std::format(fmt, __VA_ARGS__).c_str())
#else
#define command(fmt, ...) std::println(fmt, __VA_ARGS__);
#endif

#if __WIN__
#define EXTRA_CMAKE                                                            \
  " -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl "
#elif __LINUX__
std::string extra_cmake(std::string_view icpp_dir) {
  return std::format(
      " -DCMAKE_C_COMPILER={}/bin/clang -DCMAKE_CXX_COMPILER={}/bin/clang++ ",
      icpp_dir, icpp_dir);
}
#define EXTRA_CMAKE extra_cmake(icpp_dir.string())
#else
#define EXTRA_CMAKE ""
#endif

// quote the path with double quotes, to avoid issues with spaces in the path.
std::string dqpath(const fs::path &p, const fs::path &child = "") {
  return "\"" + (child.empty() ? p : p / child).string() + "\"";
}

int main(int argc, const char *argv[]) {
  auto script_path = fs::absolute(argv[0]);
  std::println("Running build script {}...", script_path.string());

  auto script_dir = script_path.parent_path();

  std::println("Phase 1: Build LLDB-SERVER...");

  auto lldb_build_dir = script_dir / "build-lldb";
  auto lldb_cmake_dir = script_dir / "cmake";
  auto lldb_server = lldb_build_dir / "lldb/bin/lldb-server";
  auto proj_root = script_dir.parent_path();
  auto aebi_root = proj_root.parent_path() / "AetherBinary";
  auto llvm_root = aebi_root / "third/llvm-project";
  if (fs::exists(lldb_server)) {
    std::println("LLDB-SERVER has already been built.");
  } else {
    if (!fs::exists(llvm_root)) {
      std::println(
          R"(The following paths should exist, you can clone https://github.com/AetherVM/AetherBinary to generate them:
    {}
    {})",
          llvm_root.string(), aebi_root.string());
      return -1;
    }
    command("cmake -S {} -B {} -G Ninja {} -DLLVM_PROJECT_ROOT={}",
            dqpath(lldb_cmake_dir), dqpath(lldb_build_dir), EXTRA_CMAKE,
            dqpath(llvm_root));
    command("cmake --build {} --target lldb-server", dqpath(lldb_build_dir));
  }

  std::println("Phase 2: Build AetherDbg...");

  auto build_type = "Release";
  auto aethervm_build_type = build_type;
  if (argc > 1) {
    build_type = argv[1];
    aethervm_build_type = build_type;
#if __WIN__
    build_type = "RelWithDebInfo";
#endif
  }

  auto build_dir = script_dir / (std::string("build") + "-" + build_type);
  if (!fs::exists(build_dir / "build.ninja")) {
    auto llvm = aebi_root / "build-llvm/install";
    auto aethervm = proj_root /
                    (std::string("build") + "-" + aethervm_build_type) /
                    "install";
    command("cmake -S {} -B {} -G Ninja -DCMAKE_BUILD_TYPE={} "
            "-DCMAKE_PREFIX_PATH=\"{};{}\" {} "
            "-DLLVM_PROJECT_ROOT={}",
            dqpath(script_dir), dqpath(build_dir), build_type, llvm.string(),
            aethervm.string(), EXTRA_CMAKE, dqpath(llvm_root));
  }
  command("cmake --build {}", dqpath(build_dir));

  std::println("Build completed.");
  return 0;
}

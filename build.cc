// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <icpp.hpp>

namespace {

std::string install_llvm;
std::string install_aebi;
std::string install_remill_deps;
std::string install_remill;
std::string this_root;
std::string build_root;
std::string_view build_type = "Release";

bool command(std::string_view cmd) {
  std::println("{}", cmd);
#if 1
  return std::system(cmd.data()) == 0;
#else
  return true;
#endif
}

std::string dqpath(std::string_view path) {
  return std::format(R"("{}")", path);
}

bool build_prepare(const fs::path &root) {
  auto aebi_root = root.parent_path() / "AetherBinary";
  auto llvm = aebi_root / "build-llvm/install";
  auto aebi = aebi_root / "build-Release/install";
  if (!fs::exists(llvm)) {
    std::println(
        R"(The following paths should exist, you can clone and build https://github.com/AetherVM/AetherBinary to generate them:
    {}
    {})",
        llvm.string(), aebi.string());
    return false;
  }
  install_llvm = llvm.string();
  install_aebi = aebi.string();
  this_root = root.string();
  build_root = (root / std::format("build-{}", build_type)).string();
  return true;
}

std::string cmake_extra(bool remill = true) {
  auto icpp_dir = fs::path(icpp::program()).parent_path().string();
  // the CLANG_PATH is for remill to build its semantics
  auto icpp_clang =
      remill ? std::format("-DCLANG_PATH={}/clang", icpp_dir) : std::string();
#if _WIN32
  if (remill)
    icpp_clang += ".exe";
  return icpp_clang +
         " -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl";
#elif __linux__
  return std::format("{} -DCMAKE_C_COMPILER={}/bin/clang "
                     "-DCMAKE_CXX_COMPILER={}/bin/clang++ ",
                     icpp_clang, icpp_dir, icpp_dir);
#else
  return icpp_clang;
#endif
}

bool cmake_init(std::string_view args, bool remill = true) {
  return command(std::format("cmake -G Ninja -DCMAKE_BUILD_TYPE={} {} {}",
                             build_type, args, cmake_extra(remill)));
}

bool cmake_build(std::string_view path) {
  for (auto &action : {"build", "install"}) {
    if (!command(std::format("cmake --{} {}", action, dqpath(path))))
      return false;
  }
  return true;
}

bool build_remill_deps() {
  auto remdeps = fs::path(build_root) / "remill-deps";
  install_remill_deps = (remdeps / "install").string();
  if (fs::exists(remdeps / "install/lib/libxed.a"))
    return true; // already built
  auto cmake = std::format(
      "-DUSE_EXTERNAL_LLVM=ON "
      "-DCMAKE_PREFIX_PATH=\"{}\" "
      "-DCMAKE_INSTALL_PREFIX={} "
      "-S {} "
      "-B {} ",
      install_llvm, dqpath(install_remill_deps),
      dqpath((fs::path(this_root) / "third/remill/dependencies").string()),
      dqpath(remdeps.string()));
  return cmake_init(cmake) ? cmake_build(remdeps.string()) : false;
}

bool build_remill() {
  auto remill = fs::path(build_root) / "remill";
  install_remill = (remill / "install").string();
  if (fs::exists(remill / "install/lib/libremill_bc.a"))
    return true; // already built
  auto cmake =
      std::format("-DLLVM_LINK_LLVM_DYLIB=ON "
                  "-DREMILL_BUILD_SPARC32_RUNTIME=OFF "
                  "-DCMAKE_PREFIX_PATH=\"{};{}\" "
                  "-DCMAKE_INSTALL_PREFIX={} "
                  "-S {} "
                  "-B {} ",
                  install_llvm, install_remill_deps, dqpath(install_remill),
                  dqpath((fs::path(this_root) / "third/remill").string()),
                  dqpath(remill.string()));
  return cmake_init(cmake) ? cmake_build(remill.string()) : false;
}

bool build_aethervm() {
  auto cmake = std::format("-DCMAKE_PREFIX_PATH=\"{};{};{}\" "
                           "-DCMAKE_INSTALL_PREFIX={} "
                           "-S {} "
                           "-B {} ",
                           install_llvm, install_aebi, install_remill,
                           dqpath((fs::path(build_root) / "install").string()),
                           dqpath(this_root), dqpath(build_root));
  return cmake_init(cmake, false) ? cmake_build(build_root) : false;
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc > 1)
    build_type = argv[1];

  if (!build_prepare(fs::absolute(argv[0]).parent_path()))
    return -1;

  if (!build_remill_deps())
    return -1;

  if (!build_remill())
    return -1;

  return build_aethervm() ? 0 : -1;
}

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <icpp.hpp>

namespace {

struct BuildConfig {
  std::string install_aebi;
  std::string install_aevm;
  std::string this_root;
  std::string build_root;
  std::string build_type = "RelWithDebInfo";

  BuildConfig(int argc, const char *argv[]) {
    auto thisroot = fs::absolute(argv[0]).parent_path();
    auto pkgroot = thisroot.parent_path().parent_path();
    auto build_dir_name = std::format("build-{}", build_type);
    install_aebi = (pkgroot / "aebi").generic_string();
    install_aevm = pkgroot.generic_string();
    this_root = thisroot.generic_string();
    build_root = (thisroot / build_dir_name).generic_string();
  }

  bool command(std::string_view cmd) {
    std::println("{}", cmd);
    return std::system(cmd.data()) == 0;
  }

  std::string dqpath(std::string_view path) {
    return std::format(R"("{}")", path);
  }

  bool cmake_init() {
    if (fs::exists(fs::path(build_root) / "build.ninja"))
      return true; // already cmaked

    auto icpp_root =
        fs::path(icpp::program()).parent_path().parent_path().generic_string();
    auto icpp_toolchain =
        (fs::path(install_aevm) / "lib/cmake/icpp.toolchain.cmake")
            .generic_string();
    if (!command(std::format(
            "cmake -G Ninja -DCMAKE_BUILD_TYPE={} "
            "-DCMAKE_PREFIX_PATH=\"{};{}\" "
            "-DICPP_INSTALL_DIR={} -DCMAKE_TOOLCHAIN_FILE={} -B {} -S {}",
            build_type, install_aebi, install_aevm, dqpath(icpp_root),
            dqpath(icpp_toolchain), dqpath(build_root), dqpath(this_root))))
      return false;

#if __WIN__
    // setup emubin running environment on Windows
    command(std::format(R"(mklink "{}/c++.dll" "{}/bin/c++.dll")", build_root,
                        icpp_root));
    command(std::format(R"(mklink "{}/LLVM-22.dll" "{}/bin/LLVM-22.dll")",
                        build_root, icpp_root));
    command(
        std::format(R"(mklink "{}/AetherBinary.dll" "{}/bin/AetherBinary.dll")",
                    build_root, install_aebi));
    command(std::format(R"(mklink "{}/AetherVM.dll" "{}/bin/AetherVM.dll")",
                        build_root, install_aevm));
#endif
    return true;
  }

  bool cmake_build() {
    return command(std::format("cmake --build {}", dqpath(build_root)));
  }
};

} // namespace

int main(int argc, const char *argv[]) {
  BuildConfig cfg(argc, argv);
  if (!cfg.cmake_init())
    return -1;

  return cfg.cmake_build() ? 0 : -1;
}

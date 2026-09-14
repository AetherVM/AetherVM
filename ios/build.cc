// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

/*
Build AetherVM for iOS in one go, usage: icpp build.cc [Debug]
*/

#include <icpp.hpp>

namespace {

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

struct BuildConfig {
  std::string install_llvm;
  std::string install_aebi;
  std::string install_remill_deps;
  std::string install_remill;
  std::string this_root;
  std::string build_root;
  std::string build_type = "Release";

  BuildConfig(int argc, const char *argv[]) {
    if (argc > 1)
      build_type = argv[1];
  }

  bool build_prepare(const fs::path &projroot, const fs::path &thisroot) {
    auto build_dir_name = std::format("build-{}", build_type);
    install_remill = (projroot / build_dir_name / "remill/install").string();
    if (!fs::exists(install_remill)) {
      std::println(
          R"(The following paths should exist, you should firstly build AetherVM.
    {})",
          install_remill);
      return false;
    }
    auto aebi_root = projroot.parent_path() / "AetherBinary";
    install_llvm = (aebi_root / "build-llvm/install").string();
    install_aebi = (aebi_root / build_dir_name / "install").string();
    install_remill_deps =
        (projroot / build_dir_name / "remill-deps/install").string();
    this_root = thisroot.string();
    build_root = (thisroot / build_dir_name).string();
    return true;
  }

  std::string cmake_extra(bool remill) {
    auto icpp_dir = fs::path(icpp::program()).parent_path().string();
    auto icpp_root = fs::path(icpp_dir).parent_path().string();
    return remill ? std::format("-DCLANG_PATH={}/clang", icpp_dir)
                  : std::string();
  }

  bool cmake_init(std::string_view args, bool remill) {
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

  bool load_libraries() {
    std::string type = std::format("build-{}", build_type);

    // load AetherBinary
    auto vm_dir = fs::path(this_root).parent_path();
    auto binary_dir = vm_dir.parent_path() / "AetherBinary";
    auto libbinary = binary_dir / type / "libAetherBinary.dylib";
    if (!icpp::load_library(libbinary.string())) {
      std::println("Failed to load {}", libbinary.string());
      return false;
    }

    // load AetherVM
    auto libvm = vm_dir / type / "libAetherVM.dylib";
    if (!icpp::load_library(libvm.string())) {
      std::println("Failed to load {}", libvm.string());
      return false;
    }

    return true;
  }

  bool build_opcode() {
    auto build_opcode = fs::path(this_root) / "build-opcode";
    auto arm64_opc_br = build_opcode / "AetherVMExt/arm64.opc.br";
    if (!fs::exists(arm64_opc_br)) {
      if (!command(std::format(
              "git clone https://github.com/AetherVM/AetherVMExt {}",
              arm64_opc_br.parent_path().string())))
        return false;
    }
    auto arm64_opc = (build_opcode / "arm64.opc").string();
    if (!fs::exists(arm64_opc)) {
      if (!command(std::format("brotli -d -o {} {}", arm64_opc,
                               arm64_opc_br.string())))
        return false;
    }
    // preload libAetherVM for handler_generator_arm64.cc
    if (!load_libraries())
      return false;

    auto arm64_opc_ret = arm64_opc + ".ret";
    if (fs::exists(arm64_opc_ret))
      return true;

    // convert .opc to .opc.ret
    const char *cvt_argv[] = {arm64_opc_ret.data()};
    return icpp::exec_source(
               (this_root + "/../tool/handler_generator_arm64.cc"),
               std::size(cvt_argv), cvt_argv) == 0;
  }

  bool build_aethervm() {
    auto aebi_build = fs::path(install_llvm).parent_path();
    auto cmake = std::format(
        "-DCMAKE_PREFIX_PATH=\"{};{};{};{}\" "
        "-DCMAKE_INSTALL_PREFIX={} "
        "-DLLVM_PROJECT_ROOT={} "
        "-DLLVM_BUILD_PATH={} "
        "-DICPP_PATH={} "
        "-DAETHER_BUILD_IOS=ON "
        "-S {} "
        "-B {} ",
        install_llvm, install_aebi, install_remill_deps, install_remill,
        dqpath((fs::path(build_root) / "install").string()),
        dqpath(((aebi_build.parent_path() / "third/llvm-project").string())),
        dqpath(((aebi_build / "llvm").string())), dqpath(icpp::program()),
        dqpath(this_root + "/.."), dqpath(build_root));
    return cmake_init(cmake, false) ? cmake_build(build_root) : false;
  }
};

} // namespace

int main(int argc, const char *argv[]) {
  BuildConfig cfg(argc, argv);
  auto thisroot = fs::absolute(argv[0]).parent_path();
  if (!cfg.build_prepare(thisroot.parent_path(), thisroot))
    return -1;

  if (!cfg.build_opcode())
    return -1;

  if (!cfg.build_aethervm())
    return -1;

  auto install = cfg.build_root + "/install";
  const char *cvt_argv[] = {install.data()};
  return icpp::exec_source((thisroot / "../tool/mac2ios.cc").string(),
                           std::size(cvt_argv), cvt_argv);
}

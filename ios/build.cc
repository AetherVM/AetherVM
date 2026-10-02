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

bool patch_file_string(std::string_view infile, std::string_view patch_flag,
                       std::string_view pattern, std::string_view replace) {
  std::stringstream buffer;
  {
    // read file
    buffer << std::ifstream(fs::path(infile), std::ios::in | std::ios::binary)
                  .rdbuf();
  }

  std::string content = buffer.str();
  std::size_t pos = 0;
  while ((pos = content.find(pattern, pos)) != std::string::npos) {
    // do the replacement
    content.replace(pos, pattern.length(), replace);
    pos += replace.length();
  }

  fs::path temp_file = infile;
  temp_file.replace_extension(".tmp");
  {
    // write file
    std::ofstream outf(temp_file,
                       std::ios::out | std::ios::binary | std::ios::trunc);
    outf.write(patch_flag.data(), patch_flag.size());
    outf.write("\n", 1);
    outf.write(content.data(), content.size());
  }

  // rename the temp as the original file
  fs::rename(temp_file, infile);
  return true;
}

bool starts_with(std::string_view infile, std::string_view flag,
                 bool *cr = nullptr) {
  std::string firstline;
  std::getline(std::ifstream(fs::path(infile), std::ios::binary), firstline);
  if (cr)
    *cr = firstline.back() == '\r';
  return firstline.starts_with(flag);
}

void lf2crlf(std::string &str) {
  std::size_t pos = 0;
  while ((pos = str.find('\n', pos)) != std::string::npos) {
    if (pos == 0 || str[pos - 1] != '\r') {
      str.replace(pos, 1, "\r\n");
      pos += 2;
    } else {
      pos += 1;
    }
  }
}

void check_patch(std::string_view infile, std::string_view patch_flag,
                 std::string_view pattern, std::string_view replace) {
  bool cr = false;
  if (!starts_with(infile, patch_flag, &cr)) {
    std::println("Patching {}...", infile);
    std::string newpat;
    if (cr && pattern.contains('\n')) {
      newpat = pattern;
      // the source code may in \r\n mode converted by git
      lf2crlf(newpat);
      pattern = newpat;
    }
    patch_file_string(infile, patch_flag, pattern, replace);
  }
}

std::string dqpath(std::string_view path) {
  return std::format(R"("{}")", path);
}

struct BuildConfig {
  std::string install_llvm;
  std::string install_aebi;
  std::string install_remill_deps;
  std::string install_remill;
  std::string aebi_root;
  std::string proj_root;
  std::string this_root;
  std::string build_root;
  std::string build_type = "Release";
  bool icpp;

  BuildConfig(int argc, const char *argv[]) {
    if (argc > 1)
      build_type = argv[1];

    icpp = fs::path(argv[0]).stem() == "build-icpp";
  }

  bool build_prepare(const fs::path &thisroot) {
    auto projroot = thisroot.parent_path();
    auto aebiroot = projroot.parent_path() / "AetherBinary";
    auto llvm = aebiroot / "build-ios-llvm/install";
    auto aebi = aebiroot / std::format("build-ios-{}/install", build_type);
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
    aebi_root = aebiroot.string();
    proj_root = projroot.string();
    this_root = thisroot.string();
    build_root =
        (thisroot / std::format("build-{}{}", icpp ? "icpp-" : "", build_type))
            .string();
    return true;
  }

  std::string cmake_extra(bool remill) {
    auto icpp_dir = fs::path(icpp::program()).parent_path().string();
    auto icpp_root = fs::path(icpp_dir).parent_path().string();
    // iOS toolchain cmake arguments
    auto toolchain_args = std::format(
        "-DCMAKE_TOOLCHAIN_FILE={}/third/ios-cmake/ios.toolchain.cmake "
        "-DCMAKE_CROSSCOMPILING=TRUE -DCMAKE_MACOSX_BUNDLE=NO -DPLATFORM=OS64 "
        "-DDEPLOYMENT_TARGET=16.5 -Wno-deprecated ",
        aebi_root);
    // the CLANG_PATH is for remill to build its semantics
    auto icpp_clang =
        remill ? std::format("-DCLANG_PATH={}/clang", icpp_dir) : std::string();
    return toolchain_args + icpp_clang;
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

  bool build_opcode() {
    auto build_opcode = fs::path(this_root) / "build-opcode";
    auto arm64_opc_br = build_opcode / "AetherVMExt/arm64.opc.br";
    if (!fs::exists(arm64_opc_br)) {
      if (!command(std::format(
              "git clone --depth=1 https://github.com/AetherVM/AetherVMExt {}",
              arm64_opc_br.parent_path().string())))
        return false;
    }
    auto arm64_opc = (build_opcode / "arm64.opc").string();
    if (!fs::exists(arm64_opc)) {
      if (!command(std::format("brotli -d -o {} {}", arm64_opc,
                               arm64_opc_br.string())))
        return false;
    }

    auto arm64_opc_ret = arm64_opc + ".ret";
    if (fs::exists(arm64_opc_ret))
      return true;

    // convert .opc to .opc.ret
    const char *cvt_argv[] = {arm64_opc_ret.data()};
    return icpp::exec_source(
               (this_root + "/../tool/handler_generator_arm64.cc"),
               std::size(cvt_argv), cvt_argv) == 0;
  }

  bool build_remill_deps() {
    auto remdeps = fs::path(build_root) / "remill-deps";
    install_remill_deps = (remdeps / "install").string();
    if (fs::exists(remdeps / "install/include/xed/xed-version.h"))
      return true; // already built

    auto remdeps_root =
        (fs::path(proj_root) / "third/remill/dependencies").string();
    auto cmake = std::format("-DUSE_EXTERNAL_LLVM=ON "
                             "-DCMAKE_PREFIX_PATH=\"{}\" "
                             "-DCMAKE_INSTALL_PREFIX={} "
                             "-S {} "
                             "-B {} ",
                             install_llvm, dqpath(install_remill_deps),
                             dqpath(remdeps_root), dqpath(remdeps.string()));
    return cmake_init(cmake, true) ? cmake_build(remdeps.string()) : false;
  }

  bool build_remill() {
    // build 1 more time with everything ready if the first time failed
    for (int i = 0; i < 2; i++) {
      auto remill = fs::path(build_root) / "remill";
      install_remill = (remill / "install").string();
      if (fs::exists(remill / "install/lib/cmake/remill/remillConfig.cmake"))
        return true; // already built

      auto icpp_root =
          fs::path(icpp::program()).parent_path().parent_path().string();
      auto remill_root = proj_root + "/third/remill";
      auto cmake =
          std::format("-DLLVM_LINK_LLVM_DYLIB=ON "
                      "-DREMILL_BUILD_SPARC32_RUNTIME=OFF "
                      "-DCMAKE_PREFIX_PATH=\"{};{}\" "
                      "-DCMAKE_INSTALL_PREFIX={} "
                      "-DREMILL_ENABLE_TESTING=OFF "
                      "-DREMILL_ENABLE_TESTING_X86=OFF "
                      "-DREMILL_ENABLE_TESTING_AARCH64=OFF "
                      "-DREMILL_ENABLE_TESTING_SLEIGH_THUMB=OFF "
                      "-DREMILL_ENABLE_TESTING_SLEIGH_PPC=OFF "
                      "-DREMILL_ENABLE_DIFFERENTIAL_TESTING=OFF "
                      "-DSLEIGH_EXECUTABLE={}/build-Release/remill/_deps/"
                      "sleigh-build/sleighspecs/spec-compiler/sleigh "
                      "-DICPP_INSTALL_DIR={} "
                      "-DCMAKE_PROJECT_INCLUDE_BEFORE={}/cmake/llvm-link.cmake "
                      "-S {} "
                      "-B {} ",
                      install_llvm, install_remill_deps, dqpath(install_remill),
                      proj_root, dqpath(icpp_root), proj_root,
                      dqpath(remill_root), dqpath(remill.string()));
      if (cmake_init(cmake, true) ? cmake_build(remill.string()) : false)
        return true;
    }
    return false;
  }

  bool build_aethervm() {
    auto aebi_build = fs::path(install_llvm).parent_path();
    auto cmake = std::format(
        "-DCMAKE_PREFIX_PATH=\"{};{};{};{}\" "
        "-DCMAKE_INSTALL_PREFIX={} "
        "-DLLVM_PROJECT_ROOT={} "
        "-DLLVM_BUILD_PATH={} "
        "-DICPP_PATH={} {} "
        "-DAETHER_BUILD_IOS=TRUE "
        "-S {} "
        "-B {} ",
        install_llvm, install_aebi, install_remill_deps, install_remill,
        dqpath((fs::path(build_root) / "install").string()),
        dqpath(((aebi_build.parent_path() / "third/llvm-project").string())),
        dqpath(((aebi_build / "llvm").string())), dqpath(icpp::program()),
        icpp ? "-DICPP_RUNTIME=ON" : "", dqpath(proj_root), dqpath(build_root));
    return cmake_init(cmake, false) ? cmake_build(build_root) : false;
  }
};

} // namespace

int main(int argc, const char *argv[]) {
  BuildConfig cfg(argc, argv);
  if (!cfg.build_prepare(fs::absolute(argv[0]).parent_path()))
    return -1;

  if (!cfg.build_opcode())
    return -1;

  if (!cfg.build_remill_deps())
    return -1;

  if (!cfg.build_remill())
    return -1;

  return cfg.build_aethervm() ? 0 : -1;
}

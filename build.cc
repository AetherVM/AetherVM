// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

/*
Build AetherVM in one go, usage: icpp build.cc [Debug]
*/

#include <icpp.hpp>

namespace {

std::string install_llvm;
std::string install_aebi;
std::string install_remill_deps;
std::string install_remill;
std::string this_root;
std::string build_root;
std::string build_type = "Release";

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

bool build_prepare(const fs::path &root) {
#if __LINUX__ || __WIN__
  // to make the c++ runtime compatible with the prebuilt icpp's c++ library
  if (build_type == "Debug")
    build_type = "RelWithDebInfo";
#endif

  auto build_dir_name = std::format("build-{}", build_type);
  auto aebi_root = root.parent_path() / "AetherBinary";
  auto llvm = aebi_root / "build-llvm/install";
  auto aebi = aebi_root / build_dir_name / "install";
  if (!fs::exists(llvm)) {
    std::println(
        R"(The following paths should exist, you can clone and build https://github.com/AetherVM/AetherBinary to generate them:
    {}
    {})",
        llvm.string(), aebi.string());
    return false;
  }
  install_llvm = llvm.generic_string();
  install_aebi = aebi.generic_string();
  this_root = root.generic_string();
  build_root = (root / build_dir_name).generic_string();
  return true;
}

std::string cmake_extra(bool remill) {
  auto icpp_dir = fs::path(icpp::program()).parent_path().generic_string();
  auto icpp_root = fs::path(icpp_dir).parent_path().generic_string();
  static bool setldenv = false;
  // the CLANG_PATH is for remill to build its semantics
  auto icpp_clang =
      remill ? std::format("-DCLANG_PATH={}/clang", icpp_dir) : std::string();
#if __WIN__
  if (!setldenv) {
    setldenv = true;
    // set rpath for the temporary tools dependent by remill
    icpp::set_env("PATH", std::format("{};{}", icpp_dir, std::getenv("PATH")));
  }
  if (remill)
    icpp_clang += ".exe";
  return std::format("{} -DICPP_INSTALL_DIR={} -DLLVM_BUILD_DIR={}/../llvm "
                     "-DCMAKE_TOOLCHAIN_FILE={}/cmake/icpp.toolchain.cmake ",
                     icpp_clang, icpp_root, install_llvm, this_root);
#elif __LINUX__
  if (!setldenv) {
    setldenv = true;
    // set rpath for the temporary tools dependent by remill
    icpp::set_env("LD_LIBRARY_PATH", std::format("{}/lib", icpp_root).c_str());
  }
  return std::format("{} -DICPP_INSTALL_DIR={} -DLLVM_BUILD_DIR={}/../llvm "
                     "-DCMAKE_TOOLCHAIN_FILE={}/cmake/icpp.toolchain.cmake ",
                     icpp_clang, icpp_root, install_llvm, this_root);
#else
  return icpp_clang;
#endif
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

bool build_remill_deps() {
  auto remdeps = fs::path(build_root) / "remill-deps";
  install_remill_deps = (remdeps / "install").generic_string();
  if (fs::exists(remdeps / "install/lib/cmake/gflags/gflags-config.cmake"))
    return true; // already built

  auto remdeps_root =
      (fs::path(this_root) / "third/remill/dependencies").generic_string();
#if __WIN__
  // patch gflags to use C99 inttypes format, otherwise it will fail to build on
  // Windows as we're using clang-cl
  check_patch(remdeps_root + "/CMakeLists.txt",
              "# PATCHED BY AetherVM build.cc",
              "52e94563eba1968783864942fedf6e87e3c611f4",
              R"(52e94563eba1968783864942fedf6e87e3c611f4
    "-DINTTYPES_FORMAT:STRING=C99"
    "-DGFLAGS_INTTYPES_FORMAT:STRING=C99")");
#endif
  auto cmake =
      std::format("-DUSE_EXTERNAL_LLVM=ON "
                  "-DCMAKE_PREFIX_PATH=\"{}\" "
                  "-DCMAKE_INSTALL_PREFIX={} "
                  "-S {} "
                  "-B {} ",
                  install_llvm, dqpath(install_remill_deps),
                  dqpath(remdeps_root), dqpath(remdeps.generic_string()));
  return cmake_init(cmake, true) ? cmake_build(remdeps.generic_string())
                                 : false;
}

bool build_remill() {
  // build 1 more time with everything ready if the first time failed
  for (int i = 0; i < 2; i++) {
    auto remill = fs::path(build_root) / "remill";
    install_remill = (remill / "install").generic_string();
    if (fs::exists(remill / "install/lib/cmake/remill/remillConfig.cmake"))
      return true; // already built

    auto remill_root = this_root + "/third/remill";
#if __WIN__
    // float80_t is not supported on Windows, patch remill to check double
    // instead
    check_patch(remill_root + "/lib/Arch/X86/Semantics/X87.cpp",
                "// PATCHED BY AetherVM build.cc",
                "#if defined(__x86_64__) || defined(__i386__) || "
                "defined(_M_X86)\n// On non-x86 architectures",
                "#if !_WIN32 && (defined(__x86_64__) || defined(__i386__) || "
                "defined(_M_X86))\n// On non-x86 architectures");
#endif
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
                    "-S {} "
                    "-B {} ",
                    install_llvm, install_remill_deps, dqpath(install_remill),
                    dqpath(remill_root), dqpath(remill.generic_string()));
    if (cmake_init(cmake, true) ? cmake_build(remill.generic_string()) : false)
      return true;
  }
  return false;
}

bool build_aethervm() {
  auto cmake = std::format(
      "-DCMAKE_PREFIX_PATH=\"{};{};{};{}\" "
      "-DCMAKE_INSTALL_PREFIX={} "
      "-DLLVM_BUILD_PATH={} "
      "-DICPP_PATH={} "
      "-S {} "
      "-B {} ",
      install_llvm, install_aebi, install_remill_deps, install_remill,
      dqpath((fs::path(build_root) / "install").generic_string()),
      dqpath(
          ((fs::path(install_llvm).parent_path() / "llvm").generic_string())),
      dqpath(icpp::program()), dqpath(this_root), dqpath(build_root));
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

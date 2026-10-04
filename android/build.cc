// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

/*
Build AetherVM for Android in one go, usage: icpp build.cc [Debug]
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

std::string toolchain_path(const std::string &cmakecache) {
  std::ifstream file(cmakecache);
  if (!file.is_open()) {
    std::cerr << "Failed to open CMakeCache.txt at: " << cmakecache
              << std::endl;
    return "";
  }
  while (file) {
    std::string line;
    std::getline(file, line);
    if (std::strstr(line.c_str(), "CMAKE_TOOLCHAIN_FILE"))
      return line.substr(line.find('=') + 1);
  }
  return "";
}

std::string common_args(std::string_view toolchain_file, std::string_view arch,
                        std::string_view libcxx_dir,
                        std::string_view install_llvm) {
  std::string link_flags = std::format("-nostdlib++ -L{}/lib", libcxx_dir);
  std::string_view target = arch == "arm64-v8a" ? "aarch64-linux-android24"
                                                : "x86_64-linux-android24";
  icpp::strings args;
  args.push_back(std::format("-DCMAKE_TOOLCHAIN_FILE={}", toolchain_file));
  args.push_back("-DCMAKE_CROSSCOMPILING=TRUE");
  args.push_back(std::format("-DLLVM_DIR={}/lib/cmake/llvm", install_llvm));
  args.push_back(std::format("-DLLVM_BUILD_DIR={}/../llvm", install_llvm));
  args.push_back("-DANDROID_STL=none");
  args.push_back("-DANDROID_PLATFORM=25");
  args.push_back(std::format("-DANDROID_ABI={}", arch));
  args.push_back(std::format("-DCMAKE_C_FLAGS=\"--target={}\"", target));
  args.push_back(std::format(
      "-DCMAKE_CXX_FLAGS=\"--target={} -nostdinc++ -nostdlib++ -fPIC "
      "-I{}/include/c++/v1\"",
      target, libcxx_dir));
  args.push_back(std::format("-DCMAKE_EXE_LINKER_FLAGS=\"{}\"", link_flags));
  args.push_back(std::format("-DCMAKE_SHARED_LINKER_FLAGS=\"{}\"", link_flags));
  args.push_back("-DCMAKE_CXX_STANDARD_LIBRARIES=\"-Wl,-Bdynamic -lc++ "
                 "-lc++abi -lunwind\"");
  args.push_back("-Wno-deprecated");

  std::string strargs;
  for (const auto &arg : args)
    strargs += arg + " ";
  return strargs;
}

struct BuildConfig {
  std::string install_llvm;
  std::string install_aebi;
  std::string install_remill_deps;
  std::string install_remill;
  std::string this_root;
  std::string proj_root;
  std::string build_root;
  std::string build_type = "Release";
  std::string build_arch = "arm64-v8a";
  std::string toolchain_file;
  std::string icpp_proj_root;
  bool icpp;

  BuildConfig(int argc, const char *argv[]) {
    if (argc == 1) {
      std::println(
          "Usage: {} -icpp=/path/to/icpp-project [-type=<Debug|Release>] "
          "[-arch=<arm64-v8a|x86_64>]",
          argv[0]);
      std::println("Default build type: Release");
      std::println("Default build arch: arm64-v8a");
      return;
    }

    for (int i = 1; i < argc; ++i) {
      std::string_view arg{argv[i]};
      if (arg.starts_with("-icpp="))
        icpp_proj_root = arg.substr(6);
      else if (arg.starts_with("-type="))
        build_type = arg.substr(6);
      else if (arg.starts_with("-arch="))
        build_arch = arg.substr(6);
    }

    icpp = fs::path(argv[0]).stem() == "build-icpp";
  }

  bool build_prepare(const fs::path &root) {
    if (!fs::exists(icpp_proj_root)) {
      std::println("The specified icpp project root does not exist: {}. Please "
                   "provide a "
                   "valid path using the -icpp=<path> argument.",
                   icpp_proj_root);
      return false;
    }

    auto projroot = root.parent_path();
    auto aebi_root = projroot.parent_path() / "AetherBinary";
    auto llvm =
        aebi_root / std::format("build-android-{}-llvm/install", build_arch);
    auto aebi = aebi_root / std::format("build-android-{}-{}/install",
                                        build_arch, build_type);
    if (!fs::exists(llvm)) {
      std::println(
          R"(The following paths should exist, you can clone and build https://github.com/AetherVM/AetherBinary to generate them:
    {}
    {})",
          llvm.string(), aebi.string());
      return false;
    }

    toolchain_file =
        toolchain_path((aebi.parent_path() / "CMakeCache.txt").string());
    if (!fs::exists(toolchain_file)) {
      std::println(
          R"(The toolchain file path could not be determined from CMakeCache.txt: {}. Please ensure that the AetherBinary project has been built correctly and that the CMakeCache.txt file exists at:
      {})",
          toolchain_file, (aebi.parent_path() / "CMakeCache.txt").string());
      return false;
    }

    install_llvm = llvm.generic_string();
    install_aebi = aebi.generic_string();
    this_root = root.generic_string();
    proj_root = projroot.generic_string();
    build_root = (root / std::format("build-{}{}-{}", icpp ? "icpp-" : "",
                                     build_arch, build_type))
                     .generic_string();
    return true;
  }

  std::string cmake_extra(bool remill) {
    auto icpp_dir = fs::path(icpp::program()).parent_path().string();
    auto icpp_root = fs::path(icpp_dir).parent_path().string();
    auto cxxconf = fs::path(icpp_proj_root) / "cmake/cxxconf";
    auto libcxx_build = cxxconf / std::format("build-{}", build_arch);
    // android toolchain cmake arguments
    auto toolchain_args = common_args(toolchain_file, build_arch,
                                      libcxx_build.string(), install_llvm);
    // the CLANG_PATH is for remill to build its semantics
    auto icpp_clang =
        remill ? std::format("-DCLANG_PATH={}/clang" EXE_EXT, icpp_dir)
               : std::string();
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

  bool build_remill_deps() {
    auto remdeps = fs::path(build_root) / "remill-deps";
    install_remill_deps = (remdeps / "install").generic_string();
    if (fs::exists(remdeps / "install/include/xed/xed-version.h"))
      return true; // already built

    auto remdeps_root =
        (fs::path(proj_root) / "android/remill-deps").generic_string();
    auto cmake =
        std::format("-DUSE_EXTERNAL_LLVM=ON "
                    "-DCMAKE_PREFIX_PATH=\"{}\" "
                    "-DCMAKE_INSTALL_PREFIX={} "
                    "-S {} "
                    "-B {} ",
                    install_llvm, dqpath(install_remill_deps),
                    dqpath(remdeps_root), dqpath(remdeps.generic_string()));
    auto result =
        cmake_init(cmake, true) ? cmake_build(remdeps.generic_string()) : false;
#if __WIN__
    // mbuild generates the wrong library names on Windows, so we need to create
    // symlinks for them
    command(std::format("cd \"{}\\lib\" && mklink libxed.a xed.lib && mklink "
                        "libxed-ild.a xed-ild.lib",
                        install_remill_deps));
#endif
    return result;
  }

  bool build_remill() {
    // build 1 more time with everything ready if the first time failed
    for (int i = 0; i < 2; i++) {
      auto remill = fs::path(build_root) / "remill";
      install_remill = (remill / "install").generic_string();
      if (fs::exists(remill / "install/lib/cmake/remill/remillConfig.cmake"))
        return true; // already built

      auto icpp_root =
          fs::path(icpp::program()).parent_path().parent_path().string();
      auto remill_root = proj_root + "/third/remill";
      auto cmake = std::format(
          "-DLLVM_LINK_LLVM_DYLIB=ON "
          "-DREMILL_BUILD_SPARC32_RUNTIME=OFF "
          "-DCMAKE_PREFIX_PATH=\"{};{}\" "
          "-DXED_DIR={}/lib/cmake/XED "
          "-Dglog_DIR={}/lib/cmake/glog "
          "-Dgflags_DIR={}/lib/cmake/gflags "
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
          install_llvm, install_remill_deps, install_remill_deps,
          install_remill_deps, install_remill_deps, dqpath(install_remill),
          proj_root, dqpath(icpp_root), proj_root, dqpath(remill_root),
          dqpath(remill.generic_string()));
      if (cmake_init(cmake, true) ? cmake_build(remill.generic_string())
                                  : false)
        return true;
    }
    return false;
  }

  bool build_aethervm() {
    auto aebi_build = fs::path(install_llvm).parent_path();
    auto cmake = std::format(
        "-DCMAKE_PREFIX_PATH=\"{};{};{};{}\" "
        "-DAetherBinary_DIR={}/lib/cmake/AetherBinary "
        "-Dremill_DIR={}/lib/cmake/remill "
        "-Dsleigh_DIR={}/lib/cmake/sleigh "
        "-DXED_DIR={}/lib/cmake/XED "
        "-Dglog_DIR={}/lib/cmake/glog "
        "-Dgflags_DIR={}/lib/cmake/gflags "
        "-DCMAKE_INSTALL_PREFIX={} "
        "-DLLVM_PROJECT_ROOT={} "
        "-DLLVM_BUILD_PATH={} "
        "-DICPP_PATH={} {} "
        "-S {} "
        "-B {} ",
        install_llvm, install_aebi, install_remill_deps, install_remill,
        install_aebi, install_remill, install_remill, install_remill_deps,
        install_remill_deps, install_remill_deps,
        dqpath((fs::path(build_root) / "install").generic_string()),
        dqpath(((aebi_build.parent_path() / "third/llvm-project")
                    .generic_string())),
        dqpath(((aebi_build / "llvm").generic_string())),
        dqpath(icpp::program()), icpp ? "-DICPP_RUNTIME=ON" : "",
        dqpath(proj_root), dqpath(build_root));
    return cmake_init(cmake, false) ? cmake_build(build_root) : false;
  }
};

} // namespace

int main(int argc, const char *argv[]) {
  BuildConfig cfg(argc, argv);
  if (!cfg.build_prepare(fs::absolute(argv[0]).parent_path()))
    return -1;

  if (!cfg.build_remill_deps())
    return -1;

  if (!cfg.build_remill())
    return -1;

  return cfg.build_aethervm() ? 0 : -1;
}

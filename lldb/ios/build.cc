// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

/*
Build AetherDbg for Android in one go, usage: icpp build.cc [Debug]
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

std::string common_args(std::string_view aebi_root) {
  return std::format(
      "-DCMAKE_TOOLCHAIN_FILE={}/third/ios-cmake/ios.toolchain.cmake "
      "-DCMAKE_CROSSCOMPILING=TRUE -DCMAKE_MACOSX_BUNDLE=NO -DPLATFORM=OS64 "
      "-DCMAKE_OSX_SYSROOT=iphoneos -DDEPLOYMENT_TARGET=16.5 -Wno-deprecated ",
      aebi_root);
}

std::string get_host_triple() {
  return std::string{icpp::arch} +
#if __WIN__
         "-pc-windows-msvc"
#elif __LINUX__
         "-pc-linux-gnu"
#else
         "-apple-darwin"

#endif
      ;
}

bool patch_string(std::string_view infile, std::string_view pattern,
                  std::string_view replace) {
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
    outf.write(content.data(), content.size());
  }

  // rename the temp as the original file
  fs::rename(temp_file, infile);
  return true;
}

void patch_build_ninja(std::string_view ninja) {
  // iPhoneSDK doesn't provide these libraries
  patch_string(ninja, "-lrt", " ");
}

struct BuildConfig {
  std::string install_llvm;
  std::string install_aebi;
  std::string install_aevm;
  std::string aebi_proj_root;
  std::string llvm_proj_root;
  std::string this_root;
  std::string proj_root;
  std::string build_root;
  std::string build_type = "Release";
  bool icpp;

  BuildConfig(int argc, const char *argv[]) {
    for (int i = 1; i < argc; ++i) {
      std::string_view arg{argv[i]};
      if (arg.starts_with("-type="))
        build_type = arg.substr(6);
    }

    icpp = fs::path(argv[0]).stem() == "build-icpp";
  }

  bool build_prepare(const fs::path &root) {
    auto projroot = root.parent_path().parent_path();
    auto aebi_root = projroot.parent_path() / "AetherBinary";
    auto llvm = aebi_root / "build-ios-llvm/install";
    auto aebi = aebi_root / std::format("build-ios-{}/install", build_type);
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
    install_aevm = (projroot / std::format("ios/build-{}{}/install",
                                           icpp ? "icpp-" : "", build_type))
                       .generic_string();
    aebi_proj_root = aebi_root.generic_string();
    llvm_proj_root = (aebi_root / "third/llvm-project").generic_string();
    this_root = root.generic_string();
    proj_root = projroot.generic_string();
    build_root =
        (root / std::format("build-{}{}", icpp ? "icpp-" : "", build_type))
            .generic_string();
    return true;
  }

  std::string cmake_extra() {
    // ios toolchain cmake arguments
    return common_args(aebi_proj_root);
  }

  bool cmake_init(std::string_view args) {
    return command(std::format(
        "cmake -G Ninja -DCMAKE_BUILD_TYPE={} -DLLVM_PROJECT_ROOT={} {} {}",
        build_type, dqpath(llvm_proj_root), args, cmake_extra()));
  }

  bool build_lldb() {
    auto lldb_build_dir = this_root + "/build-lldb";
    auto lldb_server_obj = lldb_build_dir +
                           "/lldb/tools/lldb/tools/lldb-server/CMakeFiles/"
                           "lldb-server.dir/lldb-server.cpp.o";
    // as long as this object file is built, then all the libraries we need are
    // ready
    if (fs::exists(lldb_server_obj)) {
      std::println("LLDB-SERVER has already been built.");
      return true;
    } else {
      auto args = std::format(
          "-DLLVM_TABLEGEN={}/build-llvm/llvm/bin/llvm-tblgen" EXE_EXT " "
          "-DLLVM_HOST_TRIPLE={} -B {} -S {}/../cmake "
          "-DLLDB_CAN_USE_DEBUGSERVER=TRUE "
          "-DLLDB_TOOL_DEBUGSERVER_BUILD=ON "
          "-DLLDB_CODESIGN_IDENTITY=\"-\" ",
          aebi_proj_root, get_host_triple(), lldb_build_dir, this_root);
      if (!cmake_init(args))
        return false;
      patch_build_ninja(lldb_build_dir + "/build.ninja");
      return command(std::format("cmake --build {} --target lldb-server",
                                 dqpath(lldb_build_dir)));
    }
  }

  bool build_aetherdbg() {
    auto args =
        std::format("-DCMAKE_PREFIX_PATH=\"{};{}\" "
                    "{} -B {} -S {}/.. ",
                    install_llvm, install_aevm, icpp ? "-DICPP_RUNTIME=ON" : "",
                    build_root, this_root);
    if (!cmake_init(args))
      return false;
    patch_build_ninja(build_root + "/build.ninja");
    return command(std::format("cmake --build {}", dqpath(build_root)));
  }
};

} // namespace

int main(int argc, const char *argv[]) {
  BuildConfig cfg(argc, argv);
  if (!cfg.build_prepare(fs::absolute(argv[0]).parent_path()))
    return -1;

  if (!cfg.build_lldb())
    return -1;

  return cfg.build_aetherdbg() ? 0 : -1;
}

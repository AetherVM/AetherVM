// AetherBinary - A library for MachO/ELF/PE analysis.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include "../build-Release/generated/Version.h"

#include <icpp.hpp>

namespace {

auto pack_dir(const fs::path &srcdir, const fs::path &dstroot,
              std::string_view dstname = "") {
  auto dstdir = dstroot / (dstname.size() ? dstname : srcdir.filename());
  std::error_code err;
  auto source = srcdir;
  if (fs::is_symlink(srcdir))
    source = fs::canonical(srcdir, err);
  fs::copy(source, dstdir,
           fs::copy_options::overwrite_existing | fs::copy_options::recursive |
               fs::copy_options::copy_symlinks,
           err);
  if (err)
    std::println("Failed to copy directory: {} ==> {}, {}.", source.string(),
                 dstdir.string(), err.message());
  else
    std::println("Packed directory {} from {}.", dstdir.string(),
                 source.string());
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    std::println("Usage: {} /path/to/install", argv[0]);
    return -1;
  }

  auto dstroot = fs::path(argv[1]);
  auto script_dir = fs::absolute(argv[0]).parent_path();
  auto pkgname = std::format("AetherVM-v{}-{}-{}", PROJECT_VERSION_STRING,
                             icpp::os_name, icpp::arch);
  pack_dir(script_dir / "../build-Release/install", dstroot, pkgname);

  auto targz = std::format("{}.tar.gz", pkgname);
  std::println("Packing AetherVM release package {}...", targz);
#if __APPLE__
  std::system(
      std::format("find {} -name .DS_Store -delete", dstroot.string()).data());
#endif
  std::system(
      std::format("cd {} && tar czf {} {}", dstroot.string(), targz, pkgname)
          .data());
  std::println("Created AetherVM package {}.", targz);
  return 0;
}

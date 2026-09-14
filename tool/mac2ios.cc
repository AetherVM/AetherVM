// AetherMachO - Mach-O analysis and emulation engine for macOS/iOS
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <icpp.hpp>

namespace {

// Check whether a file starts with a valid Mach-O or FAT binary magic number
bool is_macho_or_fat(const fs::path &filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open())
    return false;

  unsigned magic = 0;
  file.read(reinterpret_cast<char *>(&magic), sizeof(magic));
  if (file.gcount() < static_cast<std::streamsize>(sizeof(magic)))
    return false;

  // Common Mach-O and FAT binary magic numbers (Big & Little Endian, 64 bit)
  switch (magic) {
  case 0xFEEDFACF: // MH_MAGIC_64
  case 0xCFFAEDFE: // MH_CIGAM_64
  case 0xCAFEBABF: // FAT_MAGIC_64
  case 0xBFBAFECA: // FAT_CIGAM_64
    return true;
  default:
    return false;
  }
}

void convert_mac2ios(std::string_view approot) {
  fs::path root(approot);
  if (!fs::exists(root) || !fs::is_directory(root)) {
    std::cerr << "Invalid app root path: " << approot << '\n';
    return;
  }

  // Recursively iterate through all entries (follows symlinks if necessary)
  for (const auto &entry : fs::recursive_directory_iterator(
           root, fs::directory_options::follow_directory_symlink)) {
    if (!entry.is_regular_file())
      continue;

    const fs::path path = entry.path();
    if (is_macho_or_fat(path)) {
      // Create a temporary path for vtool output
      fs::path temp_path = path;
      temp_path += ".tmp";

      // Run vtool to update the LC_BUILD_VERSION platform struct to ios
      std::string cmd = "vtool -arch arm64 -set-build-version ios 17.0 26.4 "
                        "-replace -output \"" +
                        temp_path.string() + "\" \"" + path.string() +
                        "\" > /dev/null 2>&1";

      int ret = std::system(cmd.c_str());
      if (ret == 0 && fs::exists(temp_path)) {
        // Replace the original binary with the modified ios binary
        std::error_code ec;
        fs::rename(temp_path, path, ec);
        if (ec) {
          std::cerr << "Failed to replace " << path << ": " << ec.message()
                    << '\n';
          fs::remove(temp_path, ec);
        } else {
          std::cout << "[Converted to iOS] " << path.string() << '\n';
        }
      } else {
        // Remove temporary file if vtool failed (e.g., file didn't contain
        // LC_BUILD_VERSION)
        std::error_code ec;
        fs::remove(temp_path, ec);
      }
    }
  }
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    std::println("Usage: {} /path/to/rootdir", argv[0]);
    return -1;
  }
  convert_mac2ios(argv[1]);
  return 0;
}

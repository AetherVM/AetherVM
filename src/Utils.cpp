// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <AetherBinary.h>
#include <AetherVM.h>
#include <Utils.h>
#include <functional>
#include <vector>
#include <xxhash.h>

#include "PreEncoder.h"

namespace aether {

size_t hash_value(std::string_view str) {
  std::hash<std::string_view> hasher;
  return hasher(str);
}

uint64_t hash_buffer(const void *buffer, size_t size) {
  return (uint64_t)XXH3_64bits(buffer, size);
}

std::vector<std::string_view> string_view_split(std::string_view str,
                                                char delimiter) {
  std::vector<std::string_view> result;
  std::size_t start = 0;
  std::size_t end = str.find(delimiter);

  while (end != std::string_view::npos) {
    result.emplace_back(str.substr(start, end - start));
    start = end + 1;
    end = str.find(delimiter, start);
  }

  result.emplace_back(str.substr(start));
  return result;
}

bool encode_aevm(std::string_view path, std::string_view outpath) {
  // only iOS app bundles have a parent directory with the ".app" extension
  auto haswx = fs::path(path).parent_path().extension() != ".app";
  log_print(Runtime, "Loading and analyzing binary from path: {}...", path);
  auto bin = aether::New(path.data());
  if (!bin) {
    log_print(Runtime, "Failed to create Aether binary from path: {}", path);
    return false;
  }
  BinaryEngine engine(bin);
  PreEncoder encoder(bin, outpath, haswx);
  log_print(Runtime, "Encoding Aether VM binary...");
  auto result = encoder.encode();
  aether::Delete(bin);
  log_print(Runtime, "Finished encoding with result: {}.", outpath);
  return result;
}

void dump_aevm(std::string_view path) { PreEncoder::dump(path); }

} // namespace aether

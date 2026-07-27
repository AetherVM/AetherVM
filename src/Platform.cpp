// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Platform.h>

#if AETHER_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace aether {

size_t page_size() {
#if defined(AETHER_OS_WINDOWS)
  SYSTEM_INFO sys_info;
  GetSystemInfo(&sys_info);
  return sys_info.dwPageSize;
#elif defined(AETHER_OS_POSIX)
  long sz = sysconf(_SC_PAGESIZE);
  return (sz > 0) ? static_cast<size_t>(sz) : 4096;
#endif
}

uintptr_t page_alloc(size_t size) {
#if defined(AETHER_OS_WINDOWS)
  // Reserve 16GB of virtual address space on Windows without committing
  // physical memory
  void *reserved = ::VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);

  if (!reserved)
    return 0;
#elif defined(AETHER_OS_POSIX)
  // Portable POSIX flags (macOS + Linux)
  int flags = MAP_PRIVATE;
#if defined(MAP_ANONYMOUS)
  flags |= MAP_ANONYMOUS;
#elif defined(MAP_ANON)
  flags |= MAP_ANON;
#endif

#if defined(AETHER_OS_LINUX)
  // Don't reserve swap space for uncommitted 16GB
  flags |= MAP_NORESERVE;
#endif

  void *reserved = mmap(nullptr, size, PROT_NONE, flags, -1, 0);

  if (reserved == MAP_FAILED)
    return 0;
#endif

  return reinterpret_cast<uintptr_t>(reserved);
}

bool page_commit(void *hostptr, size_t size, bool read, bool write, bool exec) {
#if defined(AETHER_OS_WINDOWS)
  DWORD protect = PAGE_NOACCESS;
  if (exec)
    protect = (write) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
  else if (write)
    protect = PAGE_READWRITE;
  else if (read)
    protect = PAGE_READONLY;

  return ::VirtualAlloc(hostptr, size, MEM_COMMIT, protect) != nullptr;
#elif defined(AETHER_OS_POSIX)
  int prot = PROT_NONE;
  if (read)
    prot |= PROT_READ;
  if (write)
    prot |= PROT_WRITE;
  if (exec)
    prot |= PROT_EXEC;

  return mprotect(hostptr, size, prot) == 0;
#endif
}

bool page_decommit(void *hostptr, size_t size) {
#if defined(AETHER_OS_WINDOWS)
  return ::VirtualFree(hostptr, size, MEM_DECOMMIT) != FALSE;

#elif defined(AETHER_OS_POSIX)
  if (mprotect(hostptr, size, PROT_NONE) != 0)
    return false;

#if defined(AETHER_OS_LINUX)
  madvise(hostptr, size, MADV_DONTNEED);
#elif defined(AETHER_OS_MACOS)
  madvise(hostptr, size, MADV_FREE);
#endif

  return true;
#endif
}

void page_dealloc(uintptr_t pagestart, size_t size) {
  auto hostptr = reinterpret_cast<void *>(pagestart);
#if defined(AETHER_OS_WINDOWS)
  ::VirtualFree(hostptr, 0, MEM_RELEASE);
#elif defined(AETHER_OS_POSIX)
  munmap(hostptr, size);
#endif
}

std::string self_path() {
#if AETHER_OS_WINDOWS
  HMODULE hModule = NULL;
  ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&self_path), &hModule);
  std::vector<char> buffer(MAX_PATH);
  DWORD length = ::GetModuleFileNameA(hModule, buffer.data(),
                                      static_cast<DWORD>(buffer.size()));
  return std::string(buffer.data(), length);
#else
  Dl_info dli;
  dladdr((void *)self_path, &dli);
  return dli.dli_fname;
#endif
}

} // namespace aether

// AetherVM - Lift. Instrument. Emulate. Recover.
// Copyright (c) 2026 Jesse Liu <neoliu2011@gmail.com>
// SPDX-License-Identifier: Apache License, Version 2.0
// See LICENSE file in the root directory for full license text.

#include <Platform.h>
#include <Utils.h>

#if AETHER_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#if AETHER_OS_DARWIN
#include <mach/mach.h>
#include <mach/mach_vm.h>
#else
#define _GNU_SOURCE
#include <sys/uio.h>
#endif
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
  // Reserve 'size' bytes of virtual address space on Windows without committing
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
  // Don't reserve swap space for uncommitted 'size' bytes
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
#elif defined(AETHER_OS_DARWIN)
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

size_t stack_size() {
#if AETHER_OS_DARWIN
  return pthread_get_stacksize_np(pthread_self());
#elif AETHER_OS_WINDOWS
  ULONG_PTR stack_low_limit = 0;
  ULONG_PTR stack_high_limit = 0;
  ::GetCurrentThreadStackLimits(&stack_low_limit, &stack_high_limit);
  return static_cast<size_t>(stack_high_limit - stack_low_limit);
#else
  pthread_t thread = pthread_self();
  pthread_attr_t attr;
  if (pthread_getattr_np(thread, &attr) == 0) {
    size_t stack_size = 0;
    void *stack_addr = nullptr;
    pthread_attr_getstack(&attr, &stack_addr, &stack_size);
    pthread_attr_destroy(&attr);
    return stack_size;
  }
  // defualt to 2MB
  return 2 * 1024 * 1024;
#endif
}

const void *load_library(std::string_view path) {
#if AETHER_OS_WINDOWS
  auto handle = reinterpret_cast<void *>(::LoadLibraryExA(
      path.data(), nullptr,
      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS |
          LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR));
  if (handle)
    return handle;

  char buff[512];
  ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, ::GetLastError(), 0, buff, sizeof(buff), nullptr);
  log_print(Runtime, "Failed to load {}: {}", path.data(), buff);
#else
  auto handle = dlopen(path.data(), RTLD_NOW);
  if (handle)
    return handle;

  log_print(Runtime, "{}", dlerror());
#endif
  return nullptr;
}

const void *resolve_symbol(const void *handle, std::string_view name) {
#if AETHER_OS_WINDOWS
  return reinterpret_cast<const void *>(::GetProcAddress(
      reinterpret_cast<HMODULE>(const_cast<void *>(handle)), name.data()));
#else
  return dlsym(const_cast<void *>(handle), name.data());
#endif
}

int current_pid() {
#if AETHER_OS_WINDOWS
  return ::GetCurrentProcessId();
#else
  return getpid();
#endif
}

bool memory_read(uintptr_t address, void *buffer, size_t size, int32_t pid) {
  if (!buffer || size == 0)
    return false;

#if AETHER_OS_DARWIN
  task_t task = mach_task_self();
  bool deallocate_task = false;

  if (pid > 0 && pid != getpid()) {
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS)
      return false;
    deallocate_task = true;
  }

  mach_vm_size_t out_size = 0;
  kern_return_t kr = mach_vm_read_overwrite(
      task, static_cast<mach_vm_address_t>(address),
      static_cast<mach_vm_size_t>(size),
      reinterpret_cast<mach_vm_address_t>(buffer), &out_size);

  if (deallocate_task)
    mach_port_deallocate(mach_task_self(), task);

  return (kr == KERN_SUCCESS) && (out_size == size);

#elif AETHER_OS_LINUX
  pid_t target_pid = (pid > 0) ? static_cast<pid_t>(pid) : getpid();

  struct iovec local[1];
  struct iovec remote[1];

  local[0].iov_base = buffer;
  local[0].iov_len = size;

  remote[0].iov_base = reinterpret_cast<void *>(address);
  remote[0].iov_len = size;

  ssize_t bytes_read = process_vm_readv(target_pid, local, 1, remote, 1, 0);
  return bytes_read == static_cast<ssize_t>(size);

#else
  HANDLE process_handle = ::GetCurrentProcess();
  bool close_handle = false;

  if (pid > 0 && pid != static_cast<int32_t>(::GetCurrentProcessId())) {
    process_handle =
        ::OpenProcess(PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
    if (!process_handle)
      return false;
    close_handle = true;
  }

  SIZE_T bytes_read = 0;
  BOOL result =
      ::ReadProcessMemory(process_handle, reinterpret_cast<LPCVOID>(address),
                          buffer, size, &bytes_read);

  if (close_handle)
    ::CloseHandle(process_handle);

  return (result != FALSE) && (bytes_read == size);
#endif
}

bool memory_write(uintptr_t address, const void *buffer, size_t size,
                  int32_t pid) {
  if (!buffer || size == 0)
    return false;

#if AETHER_OS_DARWIN
  task_t task = mach_task_self();
  bool deallocate_task = false;

  if (pid > 0 && pid != getpid()) {
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS)
      return false;
    deallocate_task = true;
  }

  kern_return_t kr =
      mach_vm_write(task, static_cast<mach_vm_address_t>(address),
                    reinterpret_cast<vm_offset_t>(buffer),
                    static_cast<mach_msg_type_number_t>(size));

  if (deallocate_task)
    mach_port_deallocate(mach_task_self(), task);

  return kr == KERN_SUCCESS;

#elif AETHER_OS_LINUX
  pid_t target_pid = (pid > 0) ? static_cast<pid_t>(pid) : getpid();

  struct iovec local[1];
  struct iovec remote[1];

  local[0].iov_base = const_cast<void *>(buffer);
  local[0].iov_len = size;

  remote[0].iov_base = reinterpret_cast<void *>(address);
  remote[0].iov_len = size;

  ssize_t bytes_written = process_vm_writev(target_pid, local, 1, remote, 1, 0);
  return bytes_written == static_cast<ssize_t>(size);

#else
  HANDLE process_handle = ::GetCurrentProcess();
  bool close_handle = false;

  if (pid > 0 && pid != static_cast<int32_t>(::GetCurrentProcessId())) {
    process_handle = ::OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                                   FALSE, static_cast<DWORD>(pid));
    if (!process_handle)
      return false;
    close_handle = true;
  }

  SIZE_T bytes_written = 0;
  BOOL result =
      ::WriteProcessMemory(process_handle, reinterpret_cast<LPVOID>(address),
                           buffer, size, &bytes_written);

  if (close_handle)
    ::CloseHandle(process_handle);

  return (result != FALSE) && (bytes_written == size);
#endif
}

} // namespace aether

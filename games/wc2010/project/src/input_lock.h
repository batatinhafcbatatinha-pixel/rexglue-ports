#pragma once

// InputSystem's device vectors are shared by the main/input/FE threads. A named
// recursive mutex also covers the launcher and game DLL copies of PC settings.
#ifdef _WIN32
#include <windows.h>
#include <cstdio>
class Wc2010InputLock {
 public:
  Wc2010InputLock() {
    static HANDLE mutex = [] {
      wchar_t name[64];
      swprintf_s(name, L"Local\\Wc2010Input-%lu", GetCurrentProcessId());
      return CreateMutexW(nullptr, FALSE, name);
    }();
    handle_ = mutex;
    if (!handle_) std::abort();
    const DWORD result = WaitForSingleObject(handle_, INFINITE);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) std::abort();
  }
  ~Wc2010InputLock() { ReleaseMutex(handle_); }
  Wc2010InputLock(const Wc2010InputLock&) = delete;
  Wc2010InputLock& operator=(const Wc2010InputLock&) = delete;
 private:
  HANDLE handle_;
};
#else
// Windows-only workaround; other runtimes retain their input implementation.
struct Wc2010InputLock {};
#endif

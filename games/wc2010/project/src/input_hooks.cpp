// Serializes the XAM input exports. The SDK's InputSystem has no lock: every GetState/GetCapabilities/SetState/
// GetKeystroke rebuilds its device vectors (RefreshDevices), and the game calls them from several threads at once.
// Two threads in RefreshDevices freed the same vector (06/11: AV in std::_Destroy_range<DeviceInfo>, and a freed
// XObject reused by a device name string, "Xbox 360", read by KeWaitForMultipleObjects).
// Wc2010InputLock is a named recursive mutex, so the PC settings code (pc_settings.cpp) shares it.
#ifdef _WIN32
#include <cstdlib>
#include <rex/hook.h>
#include <rex/logging.h>
#include "input_lock.h"

namespace {
using Thunk = void (*)(PPCContext&, uint8_t*);

Thunk Original(const char* name) {
  HMODULE module = GetModuleHandleW(L"rexruntimerd.dll");
  if (!module) module = GetModuleHandleW(L"rexruntime.dll");
  if (!module) module = GetModuleHandleW(L"rexruntimed.dll");
  const Thunk fn = module ? reinterpret_cast<Thunk>(GetProcAddress(module, name)) : nullptr;
  if (!fn) {
    REXLOG_ERROR("Missing input runtime export {}", name);
    std::abort();
  }
  return fn;
}
}  // namespace

#define SERIAL_INPUT(name)                         \
  REX_HOOK_RAW(name) {                             \
    Wc2010InputLock lock;                          \
    static const Thunk original = Original(#name); \
    original(ctx, base);                           \
  }
SERIAL_INPUT(__imp__XamInputGetState)
SERIAL_INPUT(__imp__XamInputGetCapabilities)
SERIAL_INPUT(__imp__XamInputSetState)
SERIAL_INPUT(__imp__XamInputGetKeystrokeEx)
#endif

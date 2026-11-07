#include "../platform/native_byteorder.h"
#include "xma_commands.h"
#include "xma_engine.h"

#include <array>
#include <atomic>
#ifdef _WIN32
#include <intrin.h>
#endif

#include <rex/logging.h>
#include <rex/system/xmemory.h>

namespace m3audio {
namespace {
// Standard XDK XMA command offsets. Reference: the local SDK's
// rex/audio/xma/register_table.inc and src/audio/xma_decoder.cpp.
constexpr uint32_t kArray = 0x0600, kLockControl = 0x0601;
constexpr uint32_t kCurrent = 0x0606, kNext = 0x0607;
constexpr uint32_t kKick = 0x0650, kLock = 0x0690, kClear = 0x06A0;
std::atomic<uint32_t> next_context{0}, lock_control{0};
std::array<std::atomic<uint32_t>, 30> command_latches{};
std::atomic<bool> unknown_read{false}, unknown_write{false};

uint32_t Read(void*, void*, uint32_t address) {
  const uint32_t reg = (address & 0xFFFF) / 4;
  uint32_t value = 0;
  auto& engine = XmaEngine::Get();
  if (!engine.ready()) return 0;
  if (reg == kArray) value = engine.array_physical();
  else if (reg == kLockControl) value = lock_control.load();
  else if (reg == kCurrent) value = next_context.fetch_add(1) % XmaEngine::kContextCount;
  else if (reg == kNext) value = next_context.load() % XmaEngine::kContextCount;
  else if (reg >= kKick && reg < kKick + 10) value = command_latches[reg - kKick].load();
  else if (reg >= kLock && reg < kLock + 10) value = command_latches[10 + reg - kLock].load();
  else if (reg >= kClear && reg < kClear + 10) value = command_latches[20 + reg - kClear].load();
  else if (!unknown_read.exchange(true))
    REXLOG_WARN("[native-xma] unsupported command register read {:04X}; compatibility incomplete", reg);
  // MMIO callback values use host byte order for bus bytes. The XDK uses
  // lwbrx/stwbrx around these little-endian command words.
  return native_host::ByteSwap32(value);
}

void Write(void*, void*, uint32_t address, uint32_t bus_value) {
  const uint32_t reg = (address & 0xFFFF) / 4;
  const uint32_t value = native_host::ByteSwap32(bus_value);
  auto& engine = XmaEngine::Get();
  if (!engine.ready()) return;
  if (reg == kLockControl) {
    lock_control = value;
    return;
  }
  uint32_t first = 0, operation = 0;
  if (reg >= kKick && reg < kKick + 10) { first = reg - kKick; operation = 0; }
  else if (reg >= kLock && reg < kLock + 10) { first = reg - kLock; operation = 1; }
  else if (reg >= kClear && reg < kClear + 10) { first = reg - kClear; operation = 2; }
  else {
    if (!unknown_write.exchange(true))
      REXLOG_WARN("[native-xma] unsupported command register write {:04X}={:08X}; compatibility incomplete", reg, value);
    return;
  }
  command_latches[operation * 10 + first] = value;
  for (uint32_t bit = 0; bit < 32; ++bit) {
    if (!(value & (1u << bit))) continue;
    const int index = int(first * 32 + bit);
    if (operation == 0) engine.Kick(index);
    else if (operation == 1) engine.Disable(index, true);
    else engine.Clear(index);
  }
}
}

bool InstallXmaCommands(rex::memory::Memory* memory) {
  next_context = 0;
  lock_control = 0;
  for (auto& latch : command_latches) latch = 0;
  unknown_read = false;
  unknown_write = false;
  // Native callbacks and engine are process-lifetime objects. Shutdown stops
  // the engine; later command accesses are ignored while !ready().
  return memory->AddVirtualMappedRange(0x7FEA0000, 0xFFFF0000, 0x0000FFFF,
                                       nullptr, Read, Write);
}
}

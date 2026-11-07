#include "port_game.h"
#if PORT_GAME == PORT_GAME_BF2MC
#include "../platform/native_byteorder.h"
// bf2mc native GPU - replacements for the GPU-facing functions of the game's static Direct3D
// library (addresses and semantics: analysis/d3d-map.md, analysis/native-gpu.md).
//
// No GPU emulator is loaded (ReXApp runs in detached mode, gpu_plugin is cleared in bf2mc_app.h):
// - The D3D device runs in its built-in "GPU-less" mode (device byte 0x28C1 bit 2, the state the
//   library itself enters after a GPU hang): kickoffs are skipped and fences are written by the CPU
//   when inserted. Command buffers are still built by the original code but never executed.
// - Draw / clear / resolve / swap call the original function (CPU-side bookkeeping: shader
//   binding patches the vertex shader microcode, dirty masks, resource fences) and then the host
//   Direct3D 11 renderer, which works from the D3D device's register shadow.
// - The display controller (vblank interrupt, a few status registers the library reads) is
//   provided here; GPU callbacks the game inserts into the command stream are delivered right away
//   on the interrupt thread, since host rendering is synchronous.

#elif PORT_GAME == PORT_GAME_DORITOS
// Doritos SDK graphics entry points, matched from Madagascar 3 (analysis/native-gpu-map.md).
// CPU bookkeeping remains AOT; draws/resolve/clear/present go to host D3D11.
#else /* madagascar3 */
// Madagascar SDK graphics entry points recovered from its own decoded image.
// CPU bookkeeping remains AOT; draws/resolve/clear/present go to host D3D11.
#endif /* PORT_GAME */
#include "native_gpu.h"
#if PORT_GAME == PORT_GAME_BF2MC
#include "../pc_settings.h"

#include "generated/default/bf2mc_init.h"

#include <rex/chrono/clock.h>
#elif PORT_GAME == PORT_GAME_DORITOS
#include "generated/default/doritoscrashcourse_init.h"
#else /* madagascar3 */
#include "generated/default/madagascar3_init.h"
#endif /* PORT_GAME */
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#if PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
#include <rex/system/xmemory.h>
#endif /* PORT_GAME */
#include <rex/system/xthread.h>
#include <rex/thread.h>
#if PORT_GAME == PORT_GAME_BF2MC

#else /* madagascar3 doritos */
#include <algorithm>
#endif /* PORT_GAME */
#include <atomic>
#if PORT_GAME == PORT_GAME_BF2MC
#include <cstdlib>
#else /* madagascar3 doritos */
#endif /* PORT_GAME */
#include <chrono>
#if PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
#include <cstring>
#endif /* PORT_GAME */
#include <deque>
#include <mutex>
#if PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
#include <stdexcept>
#include <thread>
#include <cstdlib>
#endif /* PORT_GAME */

namespace {
#if PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
std::atomic<uint32_t> device{0};
std::atomic<bool> running{false};
rex::system::object_ref<rex::system::XHostThread> interrupt_thread;
void* window_handle = nullptr;
bool renderer_ready = false;
std::mutex callbacks_mutex;
struct Callback { uint32_t function, data; };
std::deque<Callback> callbacks;
thread_local m3gpu::UpDraw pending;
thread_local uint32_t pending_device = 0;
thread_local uint32_t suppress_draws = 0;
struct SuppressDraws { SuppressDraws() { ++suppress_draws; } ~SuppressDraws() { --suppress_draws; } };
#endif /* PORT_GAME */

#if PORT_GAME == PORT_GAME_BF2MC
constexpr uint32_t kInterruptHandler = 0x82482E80;  // D3D's VdSetGraphicsInterruptCallback target

std::atomic<uint32_t> g_device{0};
std::atomic<bool> g_vblank_running{false};
std::mutex g_callback_mutex;
struct GpuCallback {
  uint32_t function;
  uint32_t data;
};
std::deque<GpuCallback> g_callbacks;
bf2n::UpDraw g_up;
uint8_t* g_membase = nullptr;
void* g_hwnd = nullptr;
bool g_renderer_ready = false;

uint8_t* Mem() {
  if (!g_membase) g_membase = rex::system::kernel_state()->memory()->virtual_membase();
  return g_membase;
#else /* madagascar3 doritos */
uint8_t* Memory() { return rex::system::kernel_state()->memory()->virtual_membase(); }
uint32_t Load(uint32_t a) {
  return rex::memory::load_and_swap<uint32_t>(rex::memory::GuestPtr(Memory(), a));
#endif /* PORT_GAME */
}
#if PORT_GAME == PORT_GAME_BF2MC
uint32_t Load32(uint32_t a) {
  uint32_t v;
  std::memcpy(&v, Mem() + a + (a >= 0xE0000000u ? 0x1000u : 0u), 4);
  return native_host::ByteSwap32(v);
#else /* madagascar3 doritos */
void Store(uint32_t a, uint32_t value) {
  rex::memory::store_and_swap<uint32_t>(rex::memory::GuestPtr(Memory(), a), value);
#endif /* PORT_GAME */
}
#if PORT_GAME == PORT_GAME_BF2MC
void Store32(uint32_t a, uint32_t v) {
  v = native_host::ByteSwap32(v);
  std::memcpy(Mem() + a + (a >= 0xE0000000u ? 0x1000u : 0u), &v, 4);
}

#else /* madagascar3 doritos */
#endif /* PORT_GAME */
void EnsureRenderer() {
#if PORT_GAME == PORT_GAME_BF2MC
  if (!g_renderer_ready && g_hwnd) g_renderer_ready = bf2n::RendererInit(g_hwnd);
#elif PORT_GAME == PORT_GAME_DORITOS
  if (!renderer_ready) {
    renderer_ready = m3gpu::RendererInit(window_handle);
    if (!renderer_ready) throw std::runtime_error("Doritos native hardware D3D11 renderer unavailable");
  }
#else /* madagascar3 */
  if (!renderer_ready) {
    renderer_ready = m3gpu::RendererInit(window_handle);
    if (!renderer_ready) throw std::runtime_error("Madagascar native hardware D3D11 renderer unavailable");
  }
#endif /* PORT_GAME */
}
#if PORT_GAME == PORT_GAME_BF2MC

// --- Display controller registers (0x7FC80000) -------------------------------------------------
// The library reads only a handful of display/status registers; nothing here executes commands.

uint32_t MmioRead(void*, void*, uint32_t addr) {
  uint32_t r = (addr & 0xFFFF) / 4;
  switch (r) {
    case 0x0F00: return 0x08100748;  // RB_EDRAM_TIMING
    case 0x0F01: return 0x0000200E;  // RB_BC_CONTROL
    case 0x194C: {                   // D1MODE_V_COUNTER: same clock as display interrupts
      auto now = std::chrono::steady_clock::now().time_since_epoch();
      uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
      const uint64_t period_us = uint64_t(1e6 / pcs::GuestRefreshHz());
      return uint32_t((us % period_us) * 750 / period_us);
#else /* madagascar3 doritos */
uint32_t DisplayRead(void*, void*, uint32_t address) {
  switch ((address & 0xFFFF) / 4) {
    case 0x0F00: return 0x08100748;
    case 0x0F01: return 0x0000200E;
    case 0x194C: {
      const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count();
      return uint32_t((us % 16667) * 750 / 16667);
#endif /* PORT_GAME */
    }
#if PORT_GAME == PORT_GAME_BF2MC
    case 0x1951: return 1;                    // interrupt status: vblank
    case 0x1961: return (1280u << 16) | 720u;  // D1MODE_VIEWPORT_SIZE
#elif PORT_GAME == PORT_GAME_DORITOS
    case 0x1951: return 1;  // handler 0x823C8638 tests vblank bit0
    case 0x1961: return (1280u << 16) | 720u;
#else /* madagascar3 */
    case 0x1951: return 1;  // handler 0x822D3668 tests vblank bit0
    case 0x1961: return (1280u << 16) | 720u;
#endif /* PORT_GAME */
    default: {
#if PORT_GAME == PORT_GAME_BF2MC
      static std::atomic<int> logged{0};
      if (logged.fetch_add(1) < 32) REXLOG_INFO("bf2native: display reg read {:04X}", r);
#else /* madagascar3 doritos */
      static std::atomic<uint32_t> unknown{0};
      if (unknown.fetch_add(1) < 16) REXLOG_WARN("[m3native] display read not established: {:08X}", address);
#endif /* PORT_GAME */
      return 0;
    }
  }
}
#if PORT_GAME == PORT_GAME_BF2MC

void MmioWrite(void*, void*, uint32_t addr, uint32_t value) {
  uint32_t r = (addr & 0xFFFF) / 4;
  static std::atomic<int> logged{0};
  if (r != 0x1844 && r != 0x01C5 && logged.fetch_add(1) < 32) {
    REXLOG_INFO("bf2native: display reg write {:04X} = {:08X}", r, value);
#elif PORT_GAME == PORT_GAME_DORITOS
void DisplayWrite(void*, void*, uint32_t address, uint32_t value) {
  static std::atomic<uint32_t> count{0};
  if (count.fetch_add(1) < 16) REXLOG_DEBUG("[m3native] display write {:08X}={:08X}", address, value);
}
void StartInterrupts() {
  if (running.exchange(true)) return;
  auto* ks = rex::system::kernel_state();
  interrupt_thread = rex::system::object_ref<rex::system::XHostThread>(
      new rex::system::XHostThread(ks, 128 * 1024, 0, []() -> int {
        auto* self = rex::system::XThread::GetCurrentThread();
        self->SetActiveCpu(2);
        auto* dispatcher = rex::system::kernel_state()->function_dispatcher();
        auto next = std::chrono::steady_clock::now();
        while (running.load()) {
          const uint32_t d = device.load();
          if (d) {
            for (;;) {
              Callback cb;
              {
                std::lock_guard guard(callbacks_mutex);
                if (callbacks.empty()) break;
                cb = callbacks.front(); callbacks.pop_front();
              }
              const uint32_t block = Load(d + 0x2A94);
              if (block) {
                Store(block + 0x10, cb.function);
                Store(block + 0x14, cb.data);
                uint64_t args[] = {1, d};
                dispatcher->ExecuteInterrupt(self->thread_state(), 0x823C8638, args, 2);
              }
            }
            auto now = std::chrono::steady_clock::now();
            if (now >= next) {
              next = now + std::chrono::microseconds(16667);
              uint64_t args[] = {0, d};
              dispatcher->ExecuteInterrupt(self->thread_state(), 0x823C8638, args, 2);
            }
          }
          rex::thread::Sleep(std::chrono::milliseconds(1));
        }
        return 0;
      }));
  interrupt_thread->set_name("Doritos native VBlank");
  interrupt_thread->Create();
}
// Waits until `interval` vblank periods (1/60 s) have passed since the previous flip; a late
// frame restarts the schedule instead of rushing to catch up.
void PaceFlip(uint32_t interval) {
  using clock = std::chrono::steady_clock;
  static clock::time_point next{};
  const auto period = std::chrono::nanoseconds(16666667) * interval;
  auto now = clock::now();
  if (next.time_since_epoch().count() == 0 || now > next + period) next = now;
  while ((now = clock::now()) < next) {
    const auto left = next - now;
    if (left > std::chrono::milliseconds(2)) rex::thread::Sleep(std::chrono::milliseconds(1));
    else std::this_thread::yield();
#else /* madagascar3 */
void DisplayWrite(void*, void*, uint32_t address, uint32_t value) {
  static std::atomic<uint32_t> count{0};
  if (count.fetch_add(1) < 16) REXLOG_DEBUG("[m3native] display write {:08X}={:08X}", address, value);
}
void StartInterrupts() {
  if (running.exchange(true)) return;
  auto* ks = rex::system::kernel_state();
  interrupt_thread = rex::system::object_ref<rex::system::XHostThread>(
      new rex::system::XHostThread(ks, 128 * 1024, 0, []() -> int {
        auto* self = rex::system::XThread::GetCurrentThread();
        self->SetActiveCpu(2);
        auto* dispatcher = rex::system::kernel_state()->function_dispatcher();
        auto next = std::chrono::steady_clock::now();
        while (running.load()) {
          const uint32_t d = device.load();
          if (d) {
            for (;;) {
              Callback cb;
              {
                std::lock_guard guard(callbacks_mutex);
                if (callbacks.empty()) break;
                cb = callbacks.front(); callbacks.pop_front();
              }
              const uint32_t block = Load(d + 0x2B14);
              if (block) {
                Store(block + 0x10, cb.function);
                Store(block + 0x14, cb.data);
                uint64_t args[] = {1, d};
                dispatcher->ExecuteInterrupt(self->thread_state(), 0x822D3668, args, 2);
              }
            }
            auto now = std::chrono::steady_clock::now();
            if (now >= next) {
              next = now + std::chrono::microseconds(16667);
              uint64_t args[] = {0, d};
              dispatcher->ExecuteInterrupt(self->thread_state(), 0x822D3668, args, 2);
            }
          }
          rex::thread::Sleep(std::chrono::milliseconds(1));
        }
        return 0;
      }));
  interrupt_thread->set_name("Madagascar native VBlank");
  interrupt_thread->Create();
}
// Waits until `interval` vblank periods (1/60 s) have passed since the previous flip; a late
// frame restarts the schedule instead of rushing to catch up.
void PaceFlip(uint32_t interval) {
  using clock = std::chrono::steady_clock;
  static clock::time_point next{};
  const auto period = std::chrono::nanoseconds(16666667) * interval;
  auto now = clock::now();
  if (next.time_since_epoch().count() == 0 || now > next + period) next = now;
  while ((now = clock::now()) < next) {
    const auto left = next - now;
    if (left > std::chrono::milliseconds(2)) rex::thread::Sleep(std::chrono::milliseconds(1));
    else std::this_thread::yield();
#endif /* PORT_GAME */
  }
#if PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
  next += period;
}
void FlushVertices(uint32_t d) {
  if (!pending.pending || pending_device != d) return;
  pending.pending = false;
  if (!suppress_draws) { EnsureRenderer(); m3gpu::RendererDrawUp(d, pending); }
}
#endif /* PORT_GAME */
}

#if PORT_GAME == PORT_GAME_BF2MC
// --- Interrupt thread: 60 Hz vblank + immediate delivery of GPU callbacks ----------------------

void StartInterruptThread() {
  if (g_vblank_running.exchange(true)) return;
  auto* ks = rex::system::kernel_state();
  auto* thread = new rex::system::XHostThread(ks, 128 * 1024, 0, []() -> int {
    auto* self = rex::system::XThread::GetCurrentThread();
    self->SetActiveCpu(2);
    auto* dispatcher = rex::system::kernel_state()->function_dispatcher();
    auto next = std::chrono::steady_clock::now();
    while (g_vblank_running.load()) {
      // The rate can change while playing (settings menu): read it every tick.
      const auto period = std::chrono::microseconds(int64_t(1e6 / pcs::GuestRefreshHz()));
      uint32_t device = g_device.load();
      // GPU callbacks (InsertCallback): the hardware writes {function, data} into the callback
      // block and raises a CP interrupt (source 1) once it reaches them.
      for (;;) {
        GpuCallback cb;
        {
          std::lock_guard<std::mutex> lock(g_callback_mutex);
          if (g_callbacks.empty()) break;
          cb = g_callbacks.front();
          g_callbacks.pop_front();
        }
        uint32_t block = Load32(device + bf2n::dev::kGpuCallbackBlock);
        if (block) {
          Store32(block + 16, cb.function);
          Store32(block + 20, cb.data);
          uint64_t args[] = {1, device};
          dispatcher->ExecuteInterrupt(self->thread_state(), kInterruptHandler, args, 2);
        }
      }
      auto now = std::chrono::steady_clock::now();
      if (now >= next) {
        next += period;
        if (now - next > period * 4) next = now + period;
        uint64_t args[] = {0, device};  // source 0: vblank
        dispatcher->ExecuteInterrupt(self->thread_state(), kInterruptHandler, args, 2);
      }
      rex::thread::Sleep(std::chrono::milliseconds(1));
    }
    return 0;
  });
  thread->set_name("bf2 VBlank");
  thread->Create();
  REXLOG_INFO("bf2native: interrupt thread started (device {:08X})", g_device.load());
}

}  // namespace

namespace bf2n {

#else /* madagascar3 doritos */
namespace m3gpu {
#endif /* PORT_GAME */
void NativeGpuInstallMmio() {
#if PORT_GAME == PORT_GAME_BF2MC
  auto* memory = rex::system::kernel_state()->memory();
  memory->AddVirtualMappedRange(0x7FC80000, 0xFFFF0000, 0x0000FFFF, nullptr, MmioRead,
                                MmioWrite);
  REXLOG_INFO("bf2native: display registers mapped, no GPU plugin");
#else /* madagascar3 doritos */
  if (!rex::system::kernel_state()->memory()->AddVirtualMappedRange(
          0x7FC80000, 0xFFFF0000, 0x0000FFFF, nullptr, DisplayRead, DisplayWrite))
    throw std::runtime_error("Native display-register mapping failed");
#endif /* PORT_GAME */
}
#if PORT_GAME == PORT_GAME_BF2MC

void NativeGpuSetWindow(void* hwnd) { g_hwnd = hwnd; }

}  // namespace bf2n

// --- Device creation ---------------------------------------------------------------------------

// sub_82486178(device, params): device initialization (EDRAM, ring buffer, engines, state).
REX_HOOK_RAW(sub_82486178) {
  uint32_t device = ctx.r3.u32;
  g_device = device;
  uint8_t* flags = Mem() + device + bf2n::dev::kFlags10433;
  *flags |= 0x04;  // GPU-less mode: CPU-written fences, no kickoff
  __imp__sub_82486178(ctx, base);
  *flags |= 0x04;
  REXLOG_INFO("bf2native: D3D device {:08X} initialized (GPU-less command buffers)", device);
  EnsureRenderer();
}

// sub_82485BE8(device): VdInitializeEngines + VdSetGraphicsInterruptCallback(0x82482E80, device).
REX_HOOK_RAW(sub_82485BE8) {
  g_device = ctx.r3.u32;
  __imp__sub_82485BE8(ctx, base);
  ctx.r3.u64 = 1;
  StartInterruptThread();
}

// sub_824841C8(device, type, function, data): D3D InsertCallback.
REX_HOOK_RAW(sub_824841C8) {
  GpuCallback cb{ctx.r5.u32, ctx.r6.u32};
  __imp__sub_824841C8(ctx, base);
  if (cb.function) {
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    g_callbacks.push_back(cb);
#else /* madagascar3 doritos */
void NativeGpuSetWindow(void* hwnd) { window_handle = hwnd; }
void NativeGpuShutdown() {
  running = false;
  if (interrupt_thread) {
    uint64_t timeout = uint64_t(-5000000);
    if (interrupt_thread->Wait(0, 0, 0, &timeout) != 0) interrupt_thread->Terminate(0);
    interrupt_thread.reset();
#endif /* PORT_GAME */
  }
}
#if PORT_GAME == PORT_GAME_BF2MC

// --- Register block emitters -------------------------------------------------------------------

// sub_8248A128 / 8248A1E8 / 8248A2D8 (command buffer, dirty mask, register base, source): the state
// flush (sub_8248B538) calls them to copy dirty register blocks - constants, fetch constants - from
// the device's shadow into SET_CONSTANT packets. The packets are only ever read by a GPU; the
// native renderer reads the shadow itself, so writing them is pure overhead (several % of the
// render thread). The flush still clears its dirty masks. BF2_KEEP_PM4=1 emits them as before.
namespace {
bool KeepPm4() {
  static const bool keep = std::getenv("BF2_KEEP_PM4") != nullptr;
  return keep;
}
}  // namespace

REX_HOOK_RAW(sub_8248A128) {
  if (KeepPm4()) __imp__sub_8248A128(ctx, base);
}
REX_HOOK_RAW(sub_8248A1E8) {
  if (KeepPm4()) __imp__sub_8248A1E8(ctx, base);
}
REX_HOOK_RAW(sub_8248A2D8) {
  if (KeepPm4()) __imp__sub_8248A2D8(ctx, base);
#else /* madagascar3 doritos */
#endif /* PORT_GAME */
}

#if PORT_GAME == PORT_GAME_BF2MC
// --- Shaders -----------------------------------------------------------------------------------

// sub_8248AB00(device, patch, shader, ..., declaration [r7]): loads the vertex shader through the
// command buffer (IM_LOAD_IMMEDIATE) and patches that copy (see ImmediateVertexShader); the copy
// ends at the command buffer pointer (device+0) on return.
#else /* madagascar3 doritos */
#endif /* PORT_GAME */
REX_HOOK_RAW(FN_GPU_HOOKS) {
#if PORT_GAME == PORT_GAME_BF2MC
  uint32_t device = ctx.r3.u32, shader = ctx.r5.u32, declaration = ctx.r7.u32;
  uint32_t before = Load32(device);
  __imp__sub_8248AB00(ctx, base);
  uint32_t end = Load32(device);
  static const bool disabled = std::getenv("BF2_NO_IMMEDIATE_VS") != nullptr;  // diagnostics
  if (disabled || end == before || !shader || !declaration) return;
  uint32_t size = Load32(shader + 0x258);
  uint32_t start = end - size + 4;
  if (!size || size > 0x10000 || (size & 3)) return;
  auto& imm = bf2n::LoadedImmediateVertexShader();
  imm.device = device;
  imm.shader = shader;
  imm.declaration = declaration;
  imm.declaration_id = Load32(declaration + 32);
  const uint8_t* src = Mem() + start + (start >= 0xE0000000u ? 0x1000u : 0u);
  imm.code.assign(src, src + size);
  ++imm.generation;
  static std::atomic<int> logs{0};
  if (logs.fetch_add(1) < 8)
    REXLOG_INFO("bf2native: vertex shader {:08X} loaded through the command buffer ({} bytes at {:08X})", shader,
                size, start);
#elif PORT_GAME == PORT_GAME_DORITOS
  const uint32_t d = ctx.r3.u32;
  device = d;
  // +2B3D bit20 skips submission; bit02 writes CPU fences and bypasses waits.
  // These are established independently in 49D0/3C30/4280/4D10, not BF2 offsets.
  Memory()[d + 0x2ABD] |= 0x22;
  __imp__sub_823D92D0(ctx, base);
  Memory()[d + 0x2ABD] |= 0x22;
  const uint32_t scratch = Load(d + 0x4230);
  if (!scratch) throw std::runtime_error("Native command scratch buffer was not allocated");
  Store(d + 0x30, scratch);
  Store(d + 0x34, scratch + 0x12C0);
  Store(d + 0x38, scratch + 0x1220);
  EnsureRenderer();
  StartInterrupts();
#else /* madagascar3 */
  const uint32_t d = ctx.r3.u32;
  device = d;
  // +2B3D bit20 skips submission; bit02 writes CPU fences and bypasses waits.
  // These are established independently in 49D0/3C30/4280/4D10, not BF2 offsets.
  Memory()[d + 0x2B3D] |= 0x22;
  __imp__sub_822D8788(ctx, base);
  Memory()[d + 0x2B3D] |= 0x22;
  const uint32_t scratch = Load(d + 0x42F0);
  if (!scratch) throw std::runtime_error("Native command scratch buffer was not allocated");
  Store(d + 0x30, scratch);
  Store(d + 0x34, scratch + 0x12C0);
  Store(d + 0x38, scratch + 0x1220);
  EnsureRenderer();
  StartInterrupts();
#endif /* PORT_GAME */
}

#if PORT_GAME == PORT_GAME_BF2MC
// --- Draws -------------------------------------------------------------------------------------

// sub_8247CBE0(device, prim, base_vertex, start_index, index_count): DrawIndexedVertices.
#else /* madagascar3 doritos */
// InsertCallback(device, command_pointer, flags, function, data): "run function(data) in the GPU
// interrupt once the GPU gets here". Hooked here rather than in its wrapper sub_822D5038 because
// Swap (0x822CF298) calls it directly for the flip callback 0x822CE880.
#endif /* PORT_GAME */
REX_HOOK_RAW(FN_GPU_HOOKS_2) {
#if PORT_GAME == PORT_GAME_BF2MC
  uint32_t device = ctx.r3.u32, prim = ctx.r4.u32, start = ctx.r6.u32, count = ctx.r7.u32;
  int32_t base_vertex = ctx.r5.s32;
  __imp__sub_8247CBE0(ctx, base);
  EnsureRenderer();
  if (g_renderer_ready) bf2n::RendererDraw(device, prim, base_vertex, start, count, true);
#elif PORT_GAME == PORT_GAME_DORITOS
  const Callback cb{ctx.r6.u32, ctx.r7.u32};
  __imp__sub_823C9718(ctx, base);
  if (cb.function) { std::lock_guard guard(callbacks_mutex); callbacks.push_back(cb); }
#else /* madagascar3 */
  const Callback cb{ctx.r6.u32, ctx.r7.u32};
  __imp__sub_822D43C0(ctx, base);
  if (cb.function) { std::lock_guard guard(callbacks_mutex); callbacks.push_back(cb); }
#endif /* PORT_GAME */
}

#if PORT_GAME == PORT_GAME_BF2MC
// sub_8247C9A0(device, prim, start_vertex, vertex_count): DrawVertices.
#else /* madagascar3 doritos */
#endif /* PORT_GAME */
REX_HOOK_RAW(FN_GPU_HOOKS_3) {
#if PORT_GAME == PORT_GAME_BF2MC
  uint32_t device = ctx.r3.u32, prim = ctx.r4.u32, start = ctx.r5.u32, count = ctx.r6.u32;
  __imp__sub_8247C9A0(ctx, base);
  EnsureRenderer();
  if (g_renderer_ready) bf2n::RendererDraw(device, prim, int32_t(start), 0, count, false);
#elif PORT_GAME == PORT_GAME_DORITOS
  const uint32_t d = ctx.r3.u32, shader = ctx.r5.u32, declaration = ctx.r7.u32;
  const uint32_t variant = ctx.r10.u32;
  __imp__sub_823CB658(ctx, base);
  // This helper emits exactly one patched IM_LOAD_IMMEDIATE plus invalidate.
  const uint32_t meta = shader + Load(shader + 0x380 + variant * 8) + 0x368;
  const uint32_t bytes = Load(meta + 4), end = Load(d + 0x30);
  if (!bytes || bytes > 0x10000 || bytes & 3 || end < bytes + 8) return;
  const uint32_t begin = end - bytes + 4;
  const uint32_t header = Load(begin - 12);
  if (header >> 30 != 3 || ((header >> 8) & 0x7F) != 0x2B || Load(begin - 4) != bytes / 4)
    throw std::runtime_error("Patched native shader packet does not match recovered helper layout");
  std::lock_guard guard(m3gpu::ShaderCodeMutex());
  auto& captured = m3gpu::LoadedImmediateVertexShader();
  captured.device = d; captured.shader = shader; captured.declaration = declaration;
  const auto* source = rex::memory::GuestPtr(base, begin);
  captured.code.assign(source, source + bytes);
#else /* madagascar3 */
  const uint32_t d = ctx.r3.u32, shader = ctx.r5.u32, declaration = ctx.r7.u32;
  const uint32_t variant = ctx.r10.u32;
  __imp__sub_822DCC28(ctx, base);
  // This helper emits exactly one patched IM_LOAD_IMMEDIATE plus invalidate.
  const uint32_t meta = shader + Load(shader + 0x380 + variant * 8) + 0x368;
  const uint32_t bytes = Load(meta + 4), end = Load(d + 0x30);
  if (!bytes || bytes > 0x10000 || bytes & 3 || end < bytes + 8) return;
  const uint32_t begin = end - bytes + 4;
  const uint32_t header = Load(begin - 12);
  if (header >> 30 != 3 || ((header >> 8) & 0x7F) != 0x2B || Load(begin - 4) != bytes / 4)
    throw std::runtime_error("Patched native shader packet does not match recovered helper layout");
  std::lock_guard guard(m3gpu::ShaderCodeMutex());
  auto& captured = m3gpu::LoadedImmediateVertexShader();
  captured.device = d; captured.shader = shader; captured.declaration = declaration;
  const auto* source = rex::memory::GuestPtr(base, begin);
  captured.code.assign(source, source + bytes);
#endif /* PORT_GAME */
}

#if PORT_GAME == PORT_GAME_BF2MC
// sub_8247C288(device, prim, vertex_count, stride) -> vertex pointer: BeginVertices.
#else /* madagascar3 doritos */
#endif /* PORT_GAME */
REX_HOOK_RAW(FN_GPU_HOOKS_4) {
#if PORT_GAME == PORT_GAME_BF2MC
  bf2n::UpDraw up;
  up.prim = ctx.r4.u32;
  up.vertex_count = ctx.r5.u32;
  up.stride = ctx.r6.u32;
  __imp__sub_8247C288(ctx, base);
  up.vertices = ctx.r3.u32;
  up.pending = up.vertices != 0;
  g_up = up;
#elif PORT_GAME == PORT_GAME_DORITOS
  const uint32_t d = ctx.r3.u32, primitive = ctx.r4.u32, first = ctx.r5.u32, count = ctx.r6.u32;
  __imp__sub_823C7A60(ctx, base);
  if (!suppress_draws) { EnsureRenderer(); m3gpu::RendererDraw(d, primitive & 63, int32_t(first), 0, count, false); }
}
REX_HOOK_RAW(sub_823C7E50) {
  const uint32_t d = ctx.r3.u32, primitive = ctx.r4.u32, first = ctx.r6.u32, count = ctx.r7.u32;
  const int32_t base_vertex = ctx.r5.s32;
  __imp__sub_823C7E50(ctx, base);
  if (!suppress_draws) { EnsureRenderer(); m3gpu::RendererDraw(d, primitive & 63, base_vertex, first, count, true); }
}
REX_HOOK_RAW(sub_823C6FC8) {
  pending = m3gpu::UpDraw{}; pending_device = ctx.r3.u32;
  pending.prim = ctx.r4.u32 & 63; pending.vertex_count = ctx.r5.u32; pending.stride = ctx.r6.u32;
  __imp__sub_823C6FC8(ctx, base);
  pending.vertices = ctx.r3.u32; pending.pending = pending.vertices != 0;
}
// DrawPrimitiveUP(device, prim, count, data, stride) = BeginVertices + memcpy + the inline EndVertices
// (`lwz r11,0x3554(d); stw r11,0x30(d)`); the vertices are complete when it returns.
REX_HOOK_RAW(sub_823C7460) {
  const uint32_t d = ctx.r3.u32;
  __imp__sub_823C7460(ctx, base);
  FlushVertices(d);
}
// Clear(device, rect_count, rects*, flags, color(ARGB), z(f1), stencil); flags bits 0-3 render
// targets, 0x10 depth, 0x20 stencil. Located from Copa 2014's 0x82468830 (same XDK library).
// The library flushes a newly set render target into its register shadow (RB_COLOR_INFO) only
// inside this call, so the host clear runs after it, on the targets the clear really addresses.
REX_HOOK_RAW(sub_823BCCF8) {
  const uint32_t d = ctx.r3.u32, count = ctx.r4.u32, rects = ctx.r5.u32, flags = ctx.r6.u32;
  const uint32_t color = ctx.r7.u32, stencil = ctx.r9.u32;
  const float z = float(ctx.f1.f64);
  FlushVertices(d);
  EnsureRenderer();
  {
    SuppressDraws suppress;
    __imp__sub_823BCCF8(ctx, base);
  }
  m3gpu::RendererClear(d, count, rects, flags, color, z, stencil);
}
// BeginTiling(device, flags, tile_count, tile_rects*, clear_color(float4*), clear_z(f1),
// clear_stencil): the tiles map to one host surface of their union, cleared once.
REX_HOOK_RAW(sub_823D7730) {
  const uint32_t d = ctx.r3.u32, tiles = ctx.r5.u32, rects = ctx.r6.u32, color = ctx.r7.u32;
  const uint32_t stencil = ctx.r9.u32;
  const float z = float(ctx.f1.f64);
  FlushVertices(d);
  EnsureRenderer();
  uint32_t w = 0, h = 0;
  for (uint32_t i = 0; i < std::min<uint32_t>(tiles, 16) && rects; ++i) {
    w = std::max(w, Load(rects + i * 16 + 8));
    h = std::max(h, Load(rects + i * 16 + 12));
  }
  if (w > 8192 || h > 8192) w = h = 0;
  m3gpu::RendererSetTiling(w && h, w, h);
  {
    SuppressDraws suppress;
    __imp__sub_823D7730(ctx, base);
  }
  if (!color) return;
  uint32_t argb = 0;
  for (int i = 0; i < 4; ++i) {
    const uint32_t bits = Load(color + i * 4);
    float v;
    std::memcpy(&v, &bits, 4);
    const uint32_t c = uint32_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    argb |= c << (i == 3 ? 24 : 16 - 8 * i);
  }
  m3gpu::RendererClear(d, 0, 0, 0x31, argb, z, stencil);
}
// EndTiling(device, flags, resolve_rect*, dest_texture*, ...): resolves each tile through
// sub_823CDF10 (hooked), then the host surfaces go back to the surfaces' own size.
REX_HOOK_RAW(sub_823D7AA0) {
  const uint32_t d = ctx.r3.u32;
  FlushVertices(d);
  EnsureRenderer();
  {
    SuppressDraws suppress;
    __imp__sub_823D7AA0(ctx, base);
  }
  m3gpu::RendererSetTiling(false, 0, 0);
}
REX_HOOK_RAW(sub_823CDF10) {
  const uint32_t d = ctx.r3.u32, stencil = Load(ctx.r1.u32 + 0x5C);
  EnsureRenderer();
  m3gpu::RendererResolve(d, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32,
                         ctx.r8.u32, ctx.r9.u32, ctx.r10.u32, float(ctx.f1.f64), stencil);
  SuppressDraws suppress;
  __imp__sub_823CDF10(ctx, base);
}
// Swap(device, front_texture). The library schedules each flip for vblank [device+0x4240] (it adds
// the present interval, 2 = 30 fps for this game) while [device+0x4238] counts vblanks; the GPU
// held the flip until then. Without a GPU nothing waited and the game ran twice as fast, so the
// flip is paced here to interval x 1/60 s.
REX_HOOK_RAW(sub_823D2EC8) {
  const uint32_t d = ctx.r3.u32, front = ctx.r4.u32;
  __imp__sub_823D2EC8(ctx, base);
  EnsureRenderer();
  // Interval = growth of the flip target per swap ([device+0x4248] counts swaps); both are also
  // updated by the flip callback, so compare against the previous Swap.
  static uint32_t last_target = 0, last_swaps = 0, interval = 1;
  const uint32_t target = Load(d + 0x4180), swaps = Load(d + 0x4188);
  if (swaps != last_swaps && last_swaps) {
    const uint32_t ratio = (target - last_target + (swaps - last_swaps) / 2) / (swaps - last_swaps);
    if (ratio >= 1 && ratio <= 4) interval = ratio;
  }
  last_target = target;
  last_swaps = swaps;
  static const bool unpaced = std::getenv("DORITOS_NO_PACING") != nullptr;  // diagnostics
  if (!unpaced) PaceFlip(interval);
  m3gpu::RendererSwap(d, front);
#else /* madagascar3 */
  const uint32_t d = ctx.r3.u32, primitive = ctx.r4.u32, first = ctx.r5.u32, count = ctx.r6.u32;
  __imp__sub_822D0E20(ctx, base);
  if (!suppress_draws) { EnsureRenderer(); m3gpu::RendererDraw(d, primitive & 63, int32_t(first), 0, count, false); }
}
REX_HOOK_RAW(sub_822D1238) {
  const uint32_t d = ctx.r3.u32, primitive = ctx.r4.u32, first = ctx.r6.u32, count = ctx.r7.u32;
  const int32_t base_vertex = ctx.r5.s32;
  __imp__sub_822D1238(ctx, base);
  if (!suppress_draws) { EnsureRenderer(); m3gpu::RendererDraw(d, primitive & 63, base_vertex, first, count, true); }
}
REX_HOOK_RAW(sub_822D0960) {
  pending = m3gpu::UpDraw{}; pending_device = ctx.r3.u32;
  pending.prim = ctx.r4.u32 & 63; pending.vertex_count = ctx.r5.u32; pending.stride = ctx.r6.u32;
  __imp__sub_822D0960(ctx, base);
  pending.vertices = ctx.r3.u32; pending.pending = pending.vertices != 0;
}
REX_HOOK_RAW(sub_822D0950) {
  const uint32_t d = ctx.r3.u32;
  __imp__sub_822D0950(ctx, base);
  FlushVertices(d);
}
// Clear(device, rect_count, rects*, flags, color(ARGB), z(f1), stencil); flags bits 0-3 render
// targets, 0x10 depth, 0x20 stencil. Located from Copa 2014's 0x82468830 (same XDK library).
// The library flushes a newly set render target into its register shadow (RB_COLOR_INFO) only
// inside this call, so the host clear runs after it, on the targets the clear really addresses.
REX_HOOK_RAW(sub_822CE5C8) {
  const uint32_t d = ctx.r3.u32, count = ctx.r4.u32, rects = ctx.r5.u32, flags = ctx.r6.u32;
  const uint32_t color = ctx.r7.u32, stencil = ctx.r9.u32;
  const float z = float(ctx.f1.f64);
  FlushVertices(d);
  EnsureRenderer();
  {
    SuppressDraws suppress;
    __imp__sub_822CE5C8(ctx, base);
  }
  m3gpu::RendererClear(d, count, rects, flags, color, z, stencil);
}
// BeginTiling(device, flags, tile_count, tile_rects*, clear_color(float4*), clear_z(f1),
// clear_stencil): the tiles map to one host surface of their union, cleared once.
REX_HOOK_RAW(sub_822D0128) {
  const uint32_t d = ctx.r3.u32, tiles = ctx.r5.u32, rects = ctx.r6.u32, color = ctx.r7.u32;
  const uint32_t stencil = ctx.r9.u32;
  const float z = float(ctx.f1.f64);
  FlushVertices(d);
  EnsureRenderer();
  uint32_t w = 0, h = 0;
  for (uint32_t i = 0; i < std::min<uint32_t>(tiles, 16) && rects; ++i) {
    w = std::max(w, Load(rects + i * 16 + 8));
    h = std::max(h, Load(rects + i * 16 + 12));
  }
  if (w > 8192 || h > 8192) w = h = 0;
  m3gpu::RendererSetTiling(w && h, w, h);
  {
    SuppressDraws suppress;
    __imp__sub_822D0128(ctx, base);
  }
  if (!color) return;
  uint32_t argb = 0;
  for (int i = 0; i < 4; ++i) {
    const uint32_t bits = Load(color + i * 4);
    float v;
    std::memcpy(&v, &bits, 4);
    const uint32_t c = uint32_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    argb |= c << (i == 3 ? 24 : 16 - 8 * i);
  }
  m3gpu::RendererClear(d, 0, 0, 0x31, argb, z, stencil);
}
// EndTiling(device, flags, resolve_rect*, dest_texture*, ...): resolves each tile through
// sub_822D3238 (hooked), then the host surfaces go back to the surfaces' own size.
REX_HOOK_RAW(sub_822D0498) {
  const uint32_t d = ctx.r3.u32;
  FlushVertices(d);
  EnsureRenderer();
  {
    SuppressDraws suppress;
    __imp__sub_822D0498(ctx, base);
  }
  m3gpu::RendererSetTiling(false, 0, 0);
}
REX_HOOK_RAW(sub_822D3238) {
  const uint32_t d = ctx.r3.u32, stencil = Load(ctx.r1.u32 + 0x5C);
  EnsureRenderer();
  m3gpu::RendererResolve(d, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32,
                         ctx.r8.u32, ctx.r9.u32, ctx.r10.u32, float(ctx.f1.f64), stencil);
  SuppressDraws suppress;
  __imp__sub_822D3238(ctx, base);
}
// Swap(device, front_texture). The library schedules each flip for vblank [device+0x4240] (it adds
// the present interval, 2 = 30 fps for this game) while [device+0x4238] counts vblanks; the GPU
// held the flip until then. Without a GPU nothing waited and the game ran twice as fast, so the
// flip is paced here to interval x 1/60 s.
REX_HOOK_RAW(sub_822CF618) {
  const uint32_t d = ctx.r3.u32, front = ctx.r4.u32;
  __imp__sub_822CF618(ctx, base);
  EnsureRenderer();
  // Interval = growth of the flip target per swap ([device+0x4248] counts swaps); both are also
  // updated by the flip callback, so compare against the previous Swap.
  static uint32_t last_target = 0, last_swaps = 0, interval = 1;
  const uint32_t target = Load(d + 0x4240), swaps = Load(d + 0x4248);
  if (swaps != last_swaps && last_swaps) {
    const uint32_t ratio = (target - last_target + (swaps - last_swaps) / 2) / (swaps - last_swaps);
    if (ratio >= 1 && ratio <= 4) interval = ratio;
  }
  last_target = target;
  last_swaps = swaps;
  static const bool unpaced = std::getenv("MADAGASCAR3_NO_PACING") != nullptr;  // diagnostics
  if (!unpaced) PaceFlip(interval);
  m3gpu::RendererSwap(d, front);
#endif /* PORT_GAME */
}

#if PORT_GAME == PORT_GAME_BF2MC
// sub_8247C5B0(device, prim, base_vertex, vertex_count, index_count, index_format, stride,
//              out_indices, [stack] out_vertices): BeginIndexedVertices.
#else /* madagascar3 doritos */
// D3D tiling kicks its per-hardware-thread replay workers through a GPU callback whose data is the
// secondary command buffer (device+0x35C0). With submission disabled (+2B3D bit 0x20) the buffer
// allocator sub_822D3B48 always fails, so the data is 0 and the worker would dereference it
// (read of guest 0x4 in sub_822E6DA8). The host renderer already drew the clear; nothing to replay.
#endif /* PORT_GAME */
REX_HOOK_RAW(FN_GPU_HOOKS_5) {
#if PORT_GAME == PORT_GAME_BF2MC
  bf2n::UpDraw up;
  up.indexed = true;
  up.prim = ctx.r4.u32;
  up.base_vertex = ctx.r5.s32;
  up.vertex_count = ctx.r6.u32;
  up.index_count = ctx.r7.u32;
  up.index32 = ctx.r8.u32 == 1;
  up.stride = ctx.r9.u32;
  uint32_t out_indices = ctx.r10.u32;
  uint32_t out_vertices = Load32(ctx.r1.u32 + 0x54);
  __imp__sub_8247C5B0(ctx, base);
  if (ctx.r3.u32 == 0 && out_indices && out_vertices) {
    up.indices = Load32(out_indices);
    up.vertices = Load32(out_vertices);
    up.pending = up.indices && up.vertices;
#else /* madagascar3 doritos */
  if (!ctx.r3.u32) {
    static std::atomic<uint32_t> n{0};
    if (n.fetch_add(1) < 4) REXLOG_INFO("[m3native] tiling replay callback without command buffer skipped");
    return;
#endif /* PORT_GAME */
  }
#if PORT_GAME == PORT_GAME_BF2MC
  g_up = up;
#elif PORT_GAME == PORT_GAME_DORITOS
  __imp__sub_823D3C00(ctx, base);
#else /* madagascar3 */
  __imp__sub_822E6220(ctx, base);
#endif /* PORT_GAME */
}

#if PORT_GAME == PORT_GAME_BF2MC
static void FlushUp(uint32_t device) {
  if (!g_up.pending) return;
  g_up.pending = false;
  EnsureRenderer();
  if (g_renderer_ready) bf2n::RendererDrawUp(device, g_up);
}

// sub_8247C508(device): EndVertices / EndIndexedVertices.
REX_HOOK_RAW(sub_8247C508) {
  uint32_t device = ctx.r3.u32;
  __imp__sub_8247C508(ctx, base);
  FlushUp(device);
}

// sub_8247C528(device, prim, count, data, stride): DrawVerticesUP.
REX_HOOK_RAW(sub_8247C528) {
  uint32_t device = ctx.r3.u32;
  __imp__sub_8247C528(ctx, base);
  FlushUp(device);
}

// sub_8247C8C0: DrawIndexedVerticesUP.
REX_HOOK_RAW(sub_8247C8C0) {
  uint32_t device = ctx.r3.u32;
  __imp__sub_8247C8C0(ctx, base);
  FlushUp(device);
}

// --- Clear / Resolve / Swap --------------------------------------------------------------------

// sub_82474310(device, rect_count, rects, flags, color, f1 = z, r9 = stencil): Clear.
REX_HOOK_RAW(sub_82474310) {
  uint32_t device = ctx.r3.u32, count = ctx.r4.u32, rects = ctx.r5.u32, flags = ctx.r6.u32;
  uint32_t color = ctx.r7.u32, stencil = ctx.r9.u32;
  float z = float(ctx.f1.f64);
  EnsureRenderer();
  if (g_renderer_ready) bf2n::RendererClear(device, count, rects, flags, color, z, stencil);
  __imp__sub_82474310(ctx, base);
}

// sub_82484E48(device, flags, src_rect, dest_texture, dest_point, level, slice, clear_color,
//              f1 = clear_z, [stack] clear_stencil, params): Resolve.
REX_HOOK_RAW(sub_82484E48) {
  uint32_t device = ctx.r3.u32, flags = ctx.r4.u32, rect = ctx.r5.u32, dest = ctx.r6.u32;
  uint32_t point = ctx.r7.u32, level = ctx.r8.u32, slice = ctx.r9.u32;
  uint32_t clear_color = ctx.r10.u32;
  float z = float(ctx.f1.f64);
  // Stack parameter slots are 8 bytes from r1+0x50; clear_z takes the 9th slot (float args also
  // consume a GPR slot), clear_stencil is the 10th (low word at +0x5C, see the caller 8234AE40).
  uint32_t stencil = Load32(ctx.r1.u32 + 0x5C);
  EnsureRenderer();
  if (g_renderer_ready) {
    bf2n::RendererResolve(device, flags, rect, dest, point, level, slice, clear_color, z, stencil);
  }
  __imp__sub_82484E48(ctx, base);
}

namespace bf2_natives {
void FrameTick();  // game_natives.cpp (BF2_NATIVE_AB)
}

// sub_82472D10(device, front_buffer_texture, params): Swap.
REX_HOOK_RAW(sub_82472D10) {
  bf2_natives::FrameTick();
  uint32_t device = ctx.r3.u32, front = ctx.r4.u32;
  EnsureRenderer();
  if (g_renderer_ready) bf2n::RendererSwap(device, front);
  __imp__sub_82472D10(ctx, base);
}

#else /* madagascar3 doritos */
#endif /* PORT_GAME */
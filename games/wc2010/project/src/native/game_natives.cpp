// Native replacements for hot translated game functions (none yet for 2010 FIFA World Cup).
//
// WC2010_NATIVE_AB=1        alternate native/translated every 600 presented frames and log
//                            "[native-ab]" with the frame time of each block (covers the
//                            renderer's byte-swapped constant copy, see renderer.cpp)
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/intrinsics.h>
#include "guest_device.h"

namespace wc2010 {
namespace natives {
namespace {
bool EnvSet(const char* name) {
  const char* v = std::getenv(name);
  return v && *v && *v != '0';
}


const bool g_ab = EnvSet("WC2010_NATIVE_AB");
std::atomic<bool> g_ab_on{true};

}  // namespace

bool NativeOn() { return !g_ab || g_ab_on.load(std::memory_order_relaxed); }

namespace {
// CPU time of the calling thread in ms.
double ThreadCpuMs() {
#ifdef _WIN32
  FILETIME c, e, k, u;
  if (!GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u)) return 0;
  auto ms = [](FILETIME f) { return double((uint64_t(f.dwHighDateTime) << 32) | f.dwLowDateTime) / 1e4; };
  return ms(k) + ms(u);
#else
  timespec ts{};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
#endif
}
}  // namespace

// Called once per presented frame (renderer.cpp).
void FrameTick() {
  if (!g_ab) return;
  static uint64_t frames = 0;
  static auto start = std::chrono::steady_clock::now();
  static double cpu_start = ThreadCpuMs();
  if (++frames % 600 != 0) return;
  auto now = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(now - start).count();
  double cpu = ThreadCpuMs();
  start = now;
  bool was = g_ab_on.load(std::memory_order_relaxed);
  REXLOG_INFO("[native-ab] native={} frames=600 ms={:.1f} fps={:.2f} thread_cpu_ms/frame={:.3f}", was ? 1 : 0, ms,
              600000.0 / ms, (cpu - cpu_start) / 600.0);
  cpu_start = cpu;
  g_ab_on.store(!was, std::memory_order_relaxed);
}
}  // namespace natives
}  // namespace wc2010

// Boot diagnostics, opt-in so the original execution remains the baseline.
REX_EXTERN(__imp__sub_82C9A4B0);
REX_HOOK_RAW(sub_82C9A4B0) {
  const uint32_t object = ctx.r3.u32;
  const bool trace = std::getenv("WC2010_BOOT_TRACE") != nullptr;
  if (trace) REXLOG_INFO("[boot] init 82C9A4B0 enter object={:08X}", object);
  __imp__sub_82C9A4B0(ctx, base);
  if (trace) REXLOG_INFO("[boot] init 82C9A4B0 exit child={:08X}",
      wc2010::LoadBE32(wc2010::GuestPtr(base, object + 100)));
}

REX_EXTERN(__imp__sub_82CBD7E0);
REX_HOOK_RAW(sub_82CBD7E0) {
  const uint32_t object = wc2010::LoadBE32(base + 0x83A4648C);
  const uint32_t child = object ? wc2010::LoadBE32(wc2010::GuestPtr(base, object + 100)) : 0;
  if (!child) {
    // The hoisted `lwzx r28, child, 0x6C6B8` runs in every state but its value is used only in state 3,
    // which needs the child. With child == 0 it reads guest 0x6C6B8: back that page with read-only
    // zeros (the 360 never maps guest memory this low) so the unused load cannot fault.
    static std::atomic<bool> lowPage{false};
    if (!lowPage.exchange(true))
      VirtualAlloc(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(base) + 0x6C000u), 0x1000, MEM_COMMIT, PAGE_READONLY);
    static std::atomic<unsigned> count{0};
    if (std::getenv("WC2010_BOOT_TRACE") && count.fetch_add(1) < 8)
      REXLOG_INFO("[boot] tick before init object={:08X} state={:08X} lr={:08X}",
          object, wc2010::LoadBE32(wc2010::GuestPtr(base, ctx.r3.u32 + 4)), uint32_t(ctx.lr));
    // The guest hoists a load used only by state 2 above its state switch.
    // Before bootflow initialization, state 0 with empty queues does no work.
    // Suppress only that idle update; never fabricate the missing child object.
    const uint32_t tick = ctx.r3.u32;
    auto word = [base](uint32_t a) { return wc2010::LoadBE32(wc2010::GuestPtr(base, a)); };
    const uint32_t queue = word(tick + 0xAC);
    const bool idle = word(tick + 4) == 0 &&
        *wc2010::GuestPtr(base, tick + 0x13) == 0 &&
        word(0x83A79080) == 0 && queue && word(queue + 8) == word(queue + 0x18) &&
        (*wc2010::GuestPtr(base, tick + 0x1A) != 0 ||
         word(tick + 0xB8) == word(tick + 0xC8) ||
         *wc2010::GuestPtr(base, tick + 0x15) != 1);
    if (idle && !std::getenv("WC2010_BOOT_ORIGINAL")) return;
  }
  __imp__sub_82CBD7E0(ctx, base);
}

// A/B switch for the renderer's fast paths, and the per-frame tick that drives it.
//
// This file used to hold a hand-written native version of one hot game function (the animation pose
// sampler). It was written by following the translated game code step by step, so it is not
// distributed with this repository. In this build the function is not replaced: the translated
// function runs. A developer who has such a replacement keeps it in the git-ignored
// src/native/local/game_natives.cpp, which CMake then uses instead of this file.
//
// STREET4_NATIVE_AB=1  alternate the renderer's fast paths on/off every 600 presented frames and log
//                      "[native-ab]" with the frame time of each block (covers the renderer's
//                      byte-swapped constant copy, see renderer.cpp)

#include <atomic>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include <rex/logging.h>

namespace street4 {
namespace natives {
namespace {
bool EnvSet(const char* name) {
  const char* v = std::getenv(name);
  return v && *v && *v != '0';
}
const bool g_ab = EnvSet("STREET4_NATIVE_AB");
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
}  // namespace street4

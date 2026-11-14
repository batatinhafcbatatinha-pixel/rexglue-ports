// A/B switch for the renderer's fast paths, and the per-frame tick that drives it.
//
// This file used to hold hand-written native versions of five hot game functions (particle vertex
// writer, vec3 normalise, shader-constant store, effect-parameter lookup and effect-parameter
// commit). Those replacements were written by following the translated game code step by step, so
// they are not distributed with this repository. In this build none of those functions is replaced:
// the translated functions run. A developer who has such replacements keeps them in the git-ignored
// project/local/game_natives.cpp, which CMake then uses instead of this file.
//
// BF2_NATIVE_AB=1  alternate the renderer's fast paths on/off every 600 presented frames and log
//                  "[native-ab]" with frame time and the calling thread's CPU time

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include <rex/logging.h>

namespace bf2_natives {
namespace {
bool EnvSet(const char* name) {
  const char* v = std::getenv(name);
  return v && *v && *v != '0';
}
const bool g_ab = EnvSet("BF2_NATIVE_AB");
std::atomic<bool> g_ab_on{true};

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

bool NativeOn() { return !g_ab || g_ab_on.load(std::memory_order_relaxed); }

// Called once per presented frame (gpu_hooks.cpp, Swap).
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
}  // namespace bf2_natives

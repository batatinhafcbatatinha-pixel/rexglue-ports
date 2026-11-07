#include "port_game.h"
#include "../platform/native_byteorder.h"
#include "audio_system.h"
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC || PORT_GAME == PORT_GAME_DORITOS
#else /* madagascar3 */
#include "../pc_settings.h"
#endif /* PORT_GAME */

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#if PORT_GAME == PORT_GAME_STREET3
#else /* bf2mc madagascar3 doritos */
#include <cmath>
#endif /* PORT_GAME */
#include <cstring>
#include <thread>

#include <SDL3/SDL.h>

#include <rex/logging.h>
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#include "pc_settings.h"
#else /* madagascar3 doritos */
#endif /* PORT_GAME */
#include <rex/system/function_dispatcher.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <rex/system/xthread.h>

#include "xma_engine.h"
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
#include "xma_commands.h"
#endif /* PORT_GAME */

#if PORT_GAME == PORT_GAME_STREET3
namespace fs3audio {
#elif PORT_GAME == PORT_GAME_BF2MC
namespace bf2audio {
#else /* madagascar3 doritos */
namespace m3audio {
#endif /* PORT_GAME */

namespace {

// The output device stopped consuming (NativeAudioSystem::WorkerMain): drop the output.
std::atomic<bool> g_output_stalled{false};

NativeAudioSystem* g_system = nullptr;

// Frames kept queued on the host stream before the guest is asked for more (~32 ms).
constexpr uint32_t kTargetQueuedFrames = 6;

inline float BeFloat(const uint8_t* p) {
  uint32_t v = native_host::ByteSwap32(*reinterpret_cast<const uint32_t*>(p));
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

}  // namespace

NativeAudioSystem* AudioSystem() { return g_system; }

NativeAudioSystem::NativeAudioSystem(rex::runtime::FunctionDispatcher* dispatcher) : dispatcher_(dispatcher) {
  g_system = this;
}

NativeAudioSystem::~NativeAudioSystem() {
  Shutdown();
  if (g_system == this) g_system = nullptr;
}

X_STATUS NativeAudioSystem::Setup(rex::system::KernelState* kernel_state) {
  kernel_state_ = kernel_state;
  if (!XmaEngine::Get().Initialize(dispatcher_->memory())) return X_STATUS_UNSUCCESSFUL;
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
  if (!InstallXmaCommands(dispatcher_->memory())) {
    XmaEngine::Get().Shutdown();
    return X_STATUS_UNSUCCESSFUL;
  }
#endif /* PORT_GAME */
  if (!OpenDevice()) REXLOG_WARN("[native-audio] no audio device; frames will be discarded");
  running_ = true;
  worker_ = rex::system::object_ref<rex::system::XHostThread>(
      new rex::system::XHostThread(kernel_state, 128 * 1024, 0, [this]() {
        WorkerMain();
        return 0;
      }));
  worker_->set_name("Native Audio");
  worker_->Create();
  REXLOG_INFO("[native-audio] native audio ready ({} output channels, no APU emulation)", device_channels_);
  return X_STATUS_SUCCESS;
}

bool NativeAudioSystem::OpenDevice() {
  SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    REXLOG_ERROR("[native-audio] SDL audio init failed: {}", SDL_GetError());
    return false;
  }
  sdl_audio_ = true;
  device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (!device_) {
    REXLOG_ERROR("[native-audio] unable to open the playback device: {}", SDL_GetError());
    return false;
  }
  SDL_AudioSpec spec{};
  int frames = 0;
  if (SDL_GetAudioDeviceFormat(device_, &spec, &frames)) device_channels_ = spec.channels >= 6 ? 6 : 2;
  const char* name = SDL_GetAudioDeviceName(device_);
  REXLOG_INFO("[native-audio] device '{}': {} ch {} Hz; mixing to {} ch", name ? name : "?", spec.channels,
              spec.freq, device_channels_);
  return true;
}

uint32_t NativeAudioSystem::QueuedFrames(const Client& c) const {
  if (!c.stream) return 0;
  int bytes = SDL_GetAudioStreamQueued(c.stream);
  return bytes <= 0 ? 0 : uint32_t(bytes) / (kFrameSamples * device_channels_ * sizeof(float));
}

X_STATUS NativeAudioSystem::RegisterClient(uint32_t callback, uint32_t callback_arg, uint32_t* out_index) {
  std::lock_guard lock(lock_);
  uint32_t index = kMaxClients;
  for (uint32_t i = 0; i < kMaxClients; i++) {
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
    if (!clients_[i].in_use) {
#else /* madagascar3 doritos */
    if (!clients_[i].in_use && !clients_[i].pending_callbacks) {
#endif /* PORT_GAME */
      index = i;
      break;
    }
  }
  if (index == kMaxClients) return X_STATUS_NO_MEMORY;
  auto* memory = dispatcher_->memory();
  Client& c = clients_[index];
  // The driver passes the callback a pointer to the argument, not the argument itself.
  c.callback_arg_ptr = memory->SystemHeapAlloc(4);
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
  if (!c.callback_arg_ptr) return X_STATUS_NO_MEMORY;
#endif /* PORT_GAME */
  rex::memory::store_and_swap<uint32_t>(memory->TranslateVirtual(c.callback_arg_ptr), callback_arg);
  c.callback = callback;
  c.stream = nullptr;
  if (device_) {
    SDL_AudioSpec src{SDL_AUDIO_F32LE, int(device_channels_), int(kSampleRate)};
    c.stream = SDL_CreateAudioStream(&src, nullptr);
    if (c.stream && !SDL_BindAudioStream(device_, c.stream)) {
      REXLOG_ERROR("[native-audio] unable to bind the client stream: {}", SDL_GetError());
      SDL_DestroyAudioStream(c.stream);
      c.stream = nullptr;
    }
    if (c.stream) {
      SDL_SetAudioStreamGetCallback(
          c.stream,
          [](void* self, SDL_AudioStream*, int, int) { static_cast<NativeAudioSystem*>(self)->wake_.notify_one(); },
          this);
    }
  }
  c.in_use = true;
  *out_index = index;
  REXLOG_INFO("[native-audio] render client {} callback {:08X} arg {:08X}", index, callback, callback_arg);
  wake_.notify_one();
  return X_STATUS_SUCCESS;
}

void NativeAudioSystem::UnregisterClient(uint32_t index) {
  if (index >= kMaxClients) return;
  std::lock_guard lock(lock_);
  Client& c = clients_[index];
  if (!c.in_use) return;
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
  c.in_use = false;
#endif /* PORT_GAME */
  if (c.stream) SDL_DestroyAudioStream(c.stream);
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
  if (c.callback_arg_ptr) dispatcher_->memory()->SystemHeapFree(c.callback_arg_ptr);
  c = Client{};
#else /* madagascar3 doritos */
  c.stream = nullptr;
  if (!c.pending_callbacks) {
    if (c.callback_arg_ptr) dispatcher_->memory()->SystemHeapFree(c.callback_arg_ptr);
    c = Client{};
  }
#endif /* PORT_GAME */
}

void NativeAudioSystem::SubmitFrame(uint32_t index, uint32_t samples) {
  if (index >= kMaxClients) return;
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
  SDL_AudioStream* stream;
  {
    std::lock_guard lock(lock_);
    if (!clients_[index].in_use) return;
    stream = clients_[index].stream;
  }
#else /* madagascar3 doritos */
  // Keep the client stream alive until submission finishes. Unregister/Shutdown
  // destroy it under the same lock.
  std::lock_guard lock(lock_);
  if (!clients_[index].in_use) return;
  SDL_AudioStream* stream = clients_[index].stream;
#endif /* PORT_GAME */
  if (!stream || !samples) return;
  // Guest frame: 6 channels of 256 big-endian floats each (FL FR FC LFE SL SR), one after another.
  const uint8_t* in = dispatcher_->memory()->TranslateVirtual(samples);
  auto ch = [&](uint32_t c, uint32_t i) { return BeFloat(in + (c * kFrameSamples + i) * 4); };
  float out[kFrameSamples * kFrameChannels];
  if (device_channels_ == 6) {
    for (uint32_t i = 0; i < kFrameSamples; i++)
      for (uint32_t c = 0; c < 6; c++) out[i * 6 + c] = std::clamp(ch(c, i), -1.0f, 1.0f);
  } else {
    // Stereo fold: center and surrounds at -3 dB, normalized so a full-scale front+center mix
    // doesn't clip; LFE is dropped (as on a stereo TV output).
    constexpr float kSide = 0.70710678f, kScale = 0.58578644f;
    for (uint32_t i = 0; i < kFrameSamples; i++) {
      float mid = ch(2, i) * kSide;
      out[i * 2] = std::clamp((ch(0, i) + mid + ch(4, i) * kSide) * kScale, -1.0f, 1.0f);
      out[i * 2 + 1] = std::clamp((ch(1, i) + mid + ch(5, i) * kSide) * kScale, -1.0f, 1.0f);
    }
  }
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
  // PC settings volume (0 while the settings menu holds the game).
  SDL_SetAudioStreamGain(stream, pcs::Volume());
#elif PORT_GAME == PORT_GAME_DORITOS
  // No PC settings menu in this port yet: full host gain; game-side category gain stays in the guest mixer.
#else /* madagascar3 */
  // Host stream volume from the PC settings menu (0 while it is open); game-side category gain
  // remains in the guest mixer.
  SDL_SetAudioStreamGain(stream, pcs::Volume());
#endif /* PORT_GAME */
  if (!g_output_stalled.load(std::memory_order_relaxed))
    SDL_PutAudioStreamData(stream, out, int(kFrameSamples * device_channels_ * sizeof(float)));
  submitted_++;
#if PORT_GAME == PORT_GAME_STREET3
#else /* bf2mc madagascar3 doritos */
  float peak = 0.0f;
  for (uint32_t i = 0; i < kFrameSamples * device_channels_; i++) peak = std::max(peak, std::fabs(out[i]));
  uint32_t p = uint32_t(peak * 32767.0f), prev = peak_.load();
  while (p > prev && !peak_.compare_exchange_weak(prev, p)) {
  }
#endif /* PORT_GAME */
}

void NativeAudioSystem::WorkerMain() {
  auto* thread = rex::system::XThread::GetCurrentThread();
#if PORT_GAME == PORT_GAME_STREET3
  // FS3_AUDIO_LOG=1: per-second render driver and XMA counters.
  const char* log_env = std::getenv("FS3_AUDIO_LOG");
#elif PORT_GAME == PORT_GAME_BF2MC
  // BF2_AUDIO_LOG=1: per-second render driver and XMA counters.
  const char* log_env = std::getenv("BF2_AUDIO_LOG");
#elif PORT_GAME == PORT_GAME_DORITOS
  // DORITOS_AUDIO_LOG=1: per-second render driver and XMA counters.
  const char* log_env = std::getenv("DORITOS_AUDIO_LOG");
#else /* madagascar3 */
  // MADAGASCAR3_AUDIO_LOG=1: per-second render driver and XMA counters.
  const char* log_env = std::getenv("MADAGASCAR3_AUDIO_LOG");
#endif /* PORT_GAME */
  bool log = log_env && *log_env && *log_env != '0';
  auto log_start = std::chrono::steady_clock::now();
  uint32_t callbacks = 0;
  // An output device that stops consuming (a TV or receiver switched off) keeps the stream full,
  // and the guest would wait on its mixer callbacks forever: after 200 ms without progress the
  // worker paces by the clock, as without a device, and drops the output until it plays again.
#if PORT_GAME == PORT_GAME_STREET3
  // FS3_AUDIO_STALL_TEST=<s> pauses the device after s seconds to exercise this.
#elif PORT_GAME == PORT_GAME_BF2MC
  // BF2_AUDIO_STALL_TEST=<s> pauses the device after s seconds to exercise this.
#elif PORT_GAME == PORT_GAME_DORITOS
  // DORITOS_AUDIO_STALL_TEST=<s> pauses the device after s seconds to exercise this.
#else /* madagascar3 */
  // MADAGASCAR3_AUDIO_STALL_TEST=<s> pauses the device after s seconds to exercise this.
#endif /* PORT_GAME */
  auto last_progress = std::chrono::steady_clock::now();
  uint32_t last_queued = 0;
#if PORT_GAME == PORT_GAME_STREET3
  const char* stall_env = std::getenv("FS3_AUDIO_STALL_TEST");
#elif PORT_GAME == PORT_GAME_BF2MC
  const char* stall_env = std::getenv("BF2_AUDIO_STALL_TEST");
#elif PORT_GAME == PORT_GAME_DORITOS
  const char* stall_env = std::getenv("DORITOS_AUDIO_STALL_TEST");
#else /* madagascar3 */
  const char* stall_env = std::getenv("MADAGASCAR3_AUDIO_STALL_TEST");
#endif /* PORT_GAME */
  const double stall_test = stall_env ? std::atof(stall_env) : 0.0;
  const auto worker_start = last_progress;
  bool stall_test_done = false;
  auto clock_next = last_progress;
  while (running_) {
    if (log && std::chrono::steady_clock::now() - log_start >= std::chrono::seconds(1)) {
      auto x = XmaEngine::Get().TakeStats();
#if PORT_GAME == PORT_GAME_STREET3
      REXLOG_INFO("[native-audio] callbacks {} submitted {} queued {} | xma kicks {} frames {} failed {} blocks {} "
                  "no-output {}",
                  callbacks, submitted_.exchange(0), clients_[0].stream ? QueuedFrames(clients_[0]) : 0, x.kicks,
                  x.frames, x.failed, x.blocks, x.no_output);
#elif PORT_GAME == PORT_GAME_BF2MC
      REXLOG_INFO("[native-audio] callbacks {} submitted {} peak {} queued {} | xma kicks {} frames {} failed {} "
                  "blocks {} no-output {} peak {}",
                  callbacks, submitted_.exchange(0), peak_.exchange(0),
                  clients_[0].stream ? QueuedFrames(clients_[0]) : 0, x.kicks,
                  x.frames, x.failed, x.blocks, x.no_output, x.peak);
#else /* madagascar3 doritos */
      uint32_t queued = 0;
      {
        std::lock_guard guard(lock_);
        if (clients_[0].stream) queued = QueuedFrames(clients_[0]);
      }
      REXLOG_INFO("[native-audio] callbacks {} submitted {} peak {} queued {} | xma kicks {} frames {} failed {} "
                  "blocks {} no-output {} peak {}",
                  callbacks, submitted_.exchange(0), peak_.exchange(0),
                  queued, x.kicks,
                  x.frames, x.failed, x.blocks, x.no_output, x.peak);
#endif /* PORT_GAME */
      callbacks = 0;
      log_start = std::chrono::steady_clock::now();
    }
    bool stalled = false;
    {
      std::lock_guard guard(lock_);
      const auto now = std::chrono::steady_clock::now();
      SDL_AudioStream* main_stream = clients_[0].in_use ? clients_[0].stream : nullptr;
      if (main_stream && stall_test > 0 && !stall_test_done &&
          std::chrono::duration<double>(now - worker_start).count() >= stall_test) {
        stall_test_done = true;
        SDL_PauseAudioStreamDevice(main_stream);
      }
      const uint32_t queued = main_stream ? QueuedFrames(clients_[0]) : 0;
      if (!main_stream || queued < last_queued || queued < kTargetQueuedFrames) last_progress = now;
      last_queued = queued;
      stalled = main_stream && now - last_progress > std::chrono::milliseconds(200);
      if (stalled != g_output_stalled.load(std::memory_order_relaxed)) {
        g_output_stalled = stalled;
        REXLOG_WARN("[native-audio] output device {}", stalled ? "stalled, pacing by the clock" : "playing again");
      }
    }
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
    uint32_t due[kMaxClients][2];
#else /* madagascar3 doritos */
    uint32_t due[kMaxClients][3];
#endif /* PORT_GAME */
    uint32_t count = 0;
    {
      std::unique_lock lock(lock_);
      for (uint32_t i = 0; i < kMaxClients; i++) {
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
        const Client& c = clients_[i];
#else /* madagascar3 doritos */
        Client& c = clients_[i];
#endif /* PORT_GAME */
        // Without a device the guest is paced by the clock instead (one frame per 5.33 ms).
        if (c.in_use && c.callback && (!c.stream || stalled || QueuedFrames(c) < kTargetQueuedFrames)) {
          due[count][0] = c.callback;
          due[count][1] = c.callback_arg_ptr;
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
          due[count][2] = i;
          ++c.pending_callbacks;
#endif /* PORT_GAME */
          count++;
        }
      }
      if (!count) {
        wake_.wait_for(lock, std::chrono::milliseconds(2));
        continue;
      }
    }
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
    for (uint32_t i = 0; i < count && running_; i++) {
      uint64_t args[] = {due[i][1]};
      dispatcher_->Execute(thread->thread_state(), due[i][0], args, 1);
      callbacks++;
#else /* madagascar3 doritos */
    for (uint32_t i = 0; i < count; i++) {
      if (running_) {
        uint64_t args[] = {due[i][1]};
        dispatcher_->Execute(thread->thread_state(), due[i][0], args, 1);
        callbacks++;
      }
      std::lock_guard guard(lock_);
      Client& c = clients_[due[i][2]];
      if (--c.pending_callbacks == 0 && !c.in_use) {
        if (c.callback_arg_ptr) dispatcher_->memory()->SystemHeapFree(c.callback_arg_ptr);
        c = Client{};
      }
#endif /* PORT_GAME */
    }
    if (!device_ || stalled) {
      // One 256-sample frame per 5.333 ms, on a fixed schedule (a plain sleep after the
      // callbacks ran slower than the 48 kHz clock).
      const auto now = std::chrono::steady_clock::now();
      if (now - clock_next > std::chrono::milliseconds(50)) clock_next = now;
      clock_next += std::chrono::nanoseconds(5333333);
      std::this_thread::sleep_until(clock_next);
    }
  }
}

void NativeAudioSystem::Shutdown() {
  if (worker_) {
    running_ = false;
    wake_.notify_all();
    // The worker may be blocked inside a guest callback; don't wait forever for it.
    uint64_t timeout = uint64_t(-5000000);  // 500 ms, relative
    if (worker_->Wait(0, 0, 0, &timeout) != 0) worker_->Terminate(0);
    worker_.reset();
  }
  {
    std::lock_guard lock(lock_);
    for (auto& c : clients_) {
      if (c.stream) SDL_DestroyAudioStream(c.stream);
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
      if (c.callback_arg_ptr) dispatcher_->memory()->SystemHeapFree(c.callback_arg_ptr);
#endif /* PORT_GAME */
      c = Client{};
    }
  }
  if (device_) {
    SDL_CloseAudioDevice(device_);
    device_ = 0;
  }
  if (sdl_audio_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    sdl_audio_ = false;
  }
  XmaEngine::Get().Shutdown();
}

#if PORT_GAME == PORT_GAME_STREET3
}  // namespace fs3audio
#elif PORT_GAME == PORT_GAME_BF2MC
}  // namespace bf2audio
#else /* madagascar3 doritos */
}  // namespace m3audio
#endif /* PORT_GAME */

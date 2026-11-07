#include "port_game.h"
#if PORT_GAME == PORT_GAME_STREET3
// Native audio system for FIFA Street 3: replaces the SDK's APU (XMA decoder hardware model and
#elif PORT_GAME == PORT_GAME_BF2MC
// Native audio system for Battlefield 2: Modern Combat: replaces the SDK's APU (XMA decoder hardware model and
#elif PORT_GAME == PORT_GAME_DORITOS
// Native audio system (from the Madagascar 3 port): replaces the SDK's APU (XMA decoder hardware model and
#else /* madagascar3 */
// Native audio system for Madagascar 3: replaces the SDK's APU (XMA decoder hardware model and
#endif /* PORT_GAME */
// XAudio render driver).
//
// The kernel's audio render driver contract is small: a client registers a callback, the driver
// calls it once per 256-sample frame, and the callback hands back a frame of 6 channels (planar,
// big-endian float) with XAudioSubmitRenderDriverFrame. Here a guest-capable host thread calls the
// callback whenever the host audio stream (SDL3) runs low, and submitted frames are converted and
#if PORT_GAME == PORT_GAME_STREET3
// queued on that stream. XMA decoding is fs3audio::XmaEngine, driven from audio_exports.cpp.
#elif PORT_GAME == PORT_GAME_BF2MC
// queued on that stream. XMA decoding is bf2audio::XmaEngine, driven from audio_exports.cpp.
#else /* madagascar3 doritos */
// queued on that stream. XMA decoding is m3audio::XmaEngine, driven by imported allocation and
// the XMAPlayback command compatibility boundary.
#endif /* PORT_GAME */
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

#include <rex/system/interfaces/audio.h>
#include <rex/system/xobject.h>

struct SDL_AudioStream;

namespace rex::runtime {
class FunctionDispatcher;
}
namespace rex::system {
class XHostThread;
}

#if PORT_GAME == PORT_GAME_STREET3
namespace fs3audio {
#elif PORT_GAME == PORT_GAME_BF2MC
namespace bf2audio {
#else /* madagascar3 doritos */
namespace m3audio {
#endif /* PORT_GAME */
using rex::X_STATUS;

class NativeAudioSystem final : public rex::system::IAudioSystem {
 public:
  static constexpr uint32_t kMaxClients = 8;
  static constexpr uint32_t kFrameSamples = 256;  // per channel
  static constexpr uint32_t kFrameChannels = 6;
  static constexpr uint32_t kSampleRate = 48000;

  explicit NativeAudioSystem(rex::runtime::FunctionDispatcher* dispatcher);
  ~NativeAudioSystem() override;

  X_STATUS Setup(rex::system::KernelState* kernel_state) override;
  void Shutdown() override;

  // Render driver client API (XAudioRegisterRenderDriverClient & co.).
  X_STATUS RegisterClient(uint32_t callback, uint32_t callback_arg, uint32_t* out_index);
  void UnregisterClient(uint32_t index);
  void SubmitFrame(uint32_t index, uint32_t samples);

 private:
  struct Client {
    bool in_use = false;
    uint32_t callback = 0;
    uint32_t callback_arg_ptr = 0;  // guest dword holding the client's argument
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#else /* madagascar3 doritos */
    uint32_t pending_callbacks = 0;  // keep callback arguments alive through unregister
#endif /* PORT_GAME */
    SDL_AudioStream* stream = nullptr;
  };

  void WorkerMain();
  bool OpenDevice();
  uint32_t QueuedFrames(const Client& c) const;

  rex::runtime::FunctionDispatcher* dispatcher_;
  rex::system::KernelState* kernel_state_ = nullptr;
  uint32_t device_ = 0;
  uint32_t device_channels_ = 2;
  bool sdl_audio_ = false;

  std::mutex lock_;
  std::condition_variable wake_;
  Client clients_[kMaxClients];
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> submitted_{0};
#if PORT_GAME == PORT_GAME_STREET3
#else /* bf2mc madagascar3 doritos */
  std::atomic<uint32_t> peak_{0};  // max |sample| * 32767 since the last log line
#endif /* PORT_GAME */
  rex::system::object_ref<rex::system::XHostThread> worker_;
};

// The single instance (created by the app's audio factory).
NativeAudioSystem* AudioSystem();

#if PORT_GAME == PORT_GAME_STREET3
}  // namespace fs3audio
#elif PORT_GAME == PORT_GAME_BF2MC
}  // namespace bf2audio
#else /* madagascar3 doritos */
}  // namespace m3audio
#endif /* PORT_GAME */

#include "port_game.h"
#if PORT_GAME == PORT_GAME_STREET3
// Native XMA decoding for FIFA Street 3.
//
// The game decodes XMA (the Xbox 360's WMA Pro variant) through two clients:
// - RenderWare Audio calls the kernel's XMA* functions (audio_exports.cpp);
// - the XDK's XMAPlayback library (inside the statically linked XAudio) fills context structures
//   itself and pokes the decoder's lock/kick/clear registers; those few functions are replaced at
//   the API level as well (audio_exports.cpp), so no decoder register file exists.
//
#elif PORT_GAME == PORT_GAME_BF2MC
// Native XMA decoding for Battlefield 2: Modern Combat.
//
// The game decodes XMA (the Xbox 360's WMA Pro variant) through two clients:
// - RenderWare Audio calls the kernel's XMA* functions (audio_exports.cpp);
// - the XDK's XMAPlayback library (inside the statically linked XAudio) fills context structures
//   itself and pokes the decoder's lock/kick/clear registers; those few functions are replaced at
//   the API level as well (audio_exports.cpp), so no decoder register file exists.
//
// In Battlefield 2 only the kernel's XMA* functions are used (no XMAPlayback library).
//
#elif PORT_GAME == PORT_GAME_DORITOS
// Native XMA decoding (from the Madagascar 3 port).
// Host FFmpeg decoder adapted from the existing Battlefield native backend.
// XEX imports create/release; XMAPlayback also writes its context structures directly.
#else /* madagascar3 */
// Native XMA decoding for Madagascar 3.
// Host FFmpeg decoder adapted from the existing Battlefield native backend.
// XEX imports create/release; XMAPlayback also writes its context structures directly.
#endif /* PORT_GAME */
// A context is the 64-byte structure the game shares with the decoder (input packet buffers, an
// output ring of 256-byte blocks, loop points). It stays in guest memory, because both clients read
// and write its fields; this engine only keeps the codec state that belongs to it. A "kick" runs one
// decode pass on the calling thread: frames are decoded with FFmpeg's WMA Pro (xmaframes) decoder
// and written to the output ring as big-endian 16-bit PCM until the ring is full or the input runs
#if PORT_GAME == PORT_GAME_STREET3
// out - the same contract the hardware offers.
#else /* bf2mc madagascar3 doritos */
// out - the same contract the hardware offers. Between XMAEnableContext and XMADisableContext a
// background sweep keeps running passes on the context, as the hardware keeps decoding it.
#endif /* PORT_GAME */
#pragma once

#include <cstdint>
#include <memory>
#if PORT_GAME == PORT_GAME_BF2MC
#include <string>
#else /* street3 madagascar3 doritos */
#endif /* PORT_GAME */

namespace rex::memory {
class Memory;
}

#if PORT_GAME == PORT_GAME_STREET3
namespace fs3audio {
#elif PORT_GAME == PORT_GAME_BF2MC
namespace bf2audio {
#else /* madagascar3 doritos */
namespace m3audio {
#endif /* PORT_GAME */

class XmaEngine {
 public:
  static constexpr uint32_t kContextCount = 320;
  static constexpr uint32_t kContextSize = 64;

  static XmaEngine& Get();

  bool Initialize(rex::memory::Memory* memory);
  void Shutdown();
  bool ready() const;

  // Physical address of the context array (what the XDK reads to compute context indices).
  uint32_t array_physical() const;

  // Returns the guest (virtual) address of a free, zeroed context, or 0.
  uint32_t Allocate();
  void Release(uint32_t context);
  // Index of a context by guest virtual or physical address; -1 if it isn't one of ours.
  int IndexOf(uint32_t address) const;
  uint32_t ContextAddress(int index) const;

  // Decoder commands.
#if PORT_GAME == PORT_GAME_STREET3
  void Kick(int index);                  // one decode pass
#else /* bf2mc madagascar3 doritos */
  void Kick(int index);                  // enable + one decode pass now
  bool Disable(int index, bool wait);    // stop the sweep; false if a pass is running and !wait
#endif /* PORT_GAME */
  bool Lock(int index, bool wait);       // stop decoding; false if a pass is running and !wait
  void Clear(int index);                 // reset offsets/validity and the codec history
  bool Busy(int index) const;            // a decode pass is running right now

  // Diagnostics counters (reset by the reader).
  struct Stats {
#if PORT_GAME == PORT_GAME_STREET3
    uint32_t kicks = 0, frames = 0, failed = 0, blocks = 0, no_output = 0;
#else /* bf2mc madagascar3 doritos */
    uint32_t kicks = 0, frames = 0, failed = 0, blocks = 0, no_output = 0, peak = 0;
#endif /* PORT_GAME */
  };
  Stats TakeStats();

#if PORT_GAME == PORT_GAME_BF2MC
  // Stall diagnostics: a short per-context history of API calls and decode pass outcomes, and a
  // readable dump of the context with that history.
  void Trace(int index, const char* what, uint32_t value = 0);

  // The game waits for output its stream end requires while the context has no input left (see
  // xma_engine.cpp): releases the frame the decoder holds back, completed with silence. Returns the
  // blocks written.
  uint32_t ReleaseHeldFrame(int index);
  std::string Describe(int index);

#else /* street3 madagascar3 doritos */
#endif /* PORT_GAME */
  // Runs `fn` with the context's guest structure while no decode pass can touch it.
  template <typename Fn>
  void Edit(int index, Fn&& fn) {
    auto guard = LockContext(index);
    fn(ContextMemory(index));
  }

 private:
  XmaEngine();
  ~XmaEngine();
  struct Impl;
  struct Guard {
    Impl* impl;
    int index;
    ~Guard();
  };
  Guard LockContext(int index);
  uint8_t* ContextMemory(int index) const;
  std::unique_ptr<Impl> impl_;
};

#if PORT_GAME == PORT_GAME_STREET3
}  // namespace fs3audio
#elif PORT_GAME == PORT_GAME_BF2MC
}  // namespace bf2audio
#else /* madagascar3 doritos */
}  // namespace m3audio
#endif /* PORT_GAME */

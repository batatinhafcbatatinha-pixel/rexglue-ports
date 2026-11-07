#include "port_game.h"
#if PORT_GAME == PORT_GAME_STREET3
// Kernel audio exports and XDK audio internals implemented natively (see xma_engine.h and
// audio_system.h). These definitions take the place of the SDK's xboxkrnl audio exports, which
// drive its APU model; the game's import thunks call straight into them.
#elif PORT_GAME == PORT_GAME_BF2MC
// Kernel audio exports implemented natively (see xma_engine.h and audio_system.h). These
// definitions take the place of the SDK's xboxkrnl audio exports, which drive its APU model; the
// game's import thunks call straight into them. Battlefield 2 uses all 19 XMA* imports and the
// XAudio render driver; it has no statically linked XMAPlayback library.
#elif PORT_GAME == PORT_GAME_DORITOS
// Native audio exports (from the Madagascar 3 port).
// Override only the seven audio imports actually present in this XEX.
// XMAPlayback context commands are handled by xma_commands.cpp.
#else /* madagascar3 */
// Madagascar 3 native imported audio exports; no game execution validation yet.
// Override only the seven audio imports actually present in this XEX.
// XMAPlayback context commands are handled by xma_commands.cpp.
#endif /* PORT_GAME */

#include <atomic>
#if PORT_GAME == PORT_GAME_STREET3 || PORT_GAME == PORT_GAME_BF2MC
#include <chrono>
#else /* madagascar3 doritos */
#endif /* PORT_GAME */
#include <cstring>
#if PORT_GAME == PORT_GAME_BF2MC
#include <string>
#else /* street3 madagascar3 doritos */
#endif /* PORT_GAME */
#include <thread>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <rex/system/xtypes.h>

#if PORT_GAME == PORT_GAME_STREET3
#include "generated/default/fifastreet3_init.h"
#elif PORT_GAME == PORT_GAME_BF2MC
#include "generated/default/bf2mc_init.h"
#elif PORT_GAME == PORT_GAME_DORITOS
#include "generated/default/doritoscrashcourse_init.h"
#else /* madagascar3 */
#include "generated/default/madagascar3_init.h"
#endif /* PORT_GAME */
#include "audio_system.h"
#include "xma_engine.h"
#include "xma_fields.h"

#if PORT_GAME == PORT_GAME_STREET3
REX_EXTERN(sub_824D5990);  // XMAPlayback: reset one stream's context copy

#else /* bf2mc madagascar3 doritos */
#endif /* PORT_GAME */
using namespace rex;
#if PORT_GAME == PORT_GAME_STREET3
using fs3audio::Ctx;
using fs3audio::XmaEngine;
namespace xf = fs3audio;
#elif PORT_GAME == PORT_GAME_BF2MC
using bf2audio::Ctx;
using bf2audio::XmaEngine;
namespace xf = bf2audio;
#else /* madagascar3 doritos */
using m3audio::Ctx;
using m3audio::XmaEngine;
namespace xf = m3audio;
#endif /* PORT_GAME */

namespace {

inline uint32_t Load32(uint8_t* base, uint32_t a) { return rex::memory::load_and_swap<uint32_t>(rex::memory::GuestPtr(base, a)); }
inline void Store32(uint8_t* base, uint32_t a, uint32_t v) { rex::memory::store_and_swap<uint32_t>(rex::memory::GuestPtr(base, a), v); }
inline uint16_t Load16(uint8_t* base, uint32_t a) { return rex::memory::load_and_swap<uint16_t>(rex::memory::GuestPtr(base, a)); }
inline void Store16(uint8_t* base, uint32_t a, uint16_t v) { rex::memory::store_and_swap<uint16_t>(rex::memory::GuestPtr(base, a), v); }
#if PORT_GAME == PORT_GAME_BF2MC

uint32_t Physical(uint32_t virtual_address) {
  return virtual_address ? REX_KERNEL_MEMORY()->GetPhysicalAddress(virtual_address) : 0;
}

int ContextIndex(uint32_t context) {
  int index = XmaEngine::Get().IndexOf(context);
  if (index < 0) {
    static std::atomic<bool> warned{false};
    if (!warned.exchange(true)) REXLOG_WARN("[native-xma] call with a foreign context {:08X}", context);
  }
  return index;
}

// Field read/modify on a context while no decode pass runs.
template <typename Fn>
void EditContext(uint32_t context, Fn&& fn) {
  int index = ContextIndex(context);
  if (index < 0) return;
  XmaEngine::Get().Edit(index, [&](uint8_t* p) {
    Ctx c;
    c.Load(p);
    fn(c);
    c.Store(p);
  });
}

void TraceCall(uint32_t context, const char* what, uint32_t value = 0) {
  XmaEngine::Get().Trace(XmaEngine::Get().IndexOf(context), what, value);
}

uint32_t ReadField(uint8_t* base, uint32_t context, xf::Field f) {
  Ctx c;
  c.Load(rex::memory::GuestPtr(base, context));
  return c[f];
}
#else /* street3 madagascar3 doritos */
#endif /* PORT_GAME */

#if PORT_GAME == PORT_GAME_STREET3
uint32_t Physical(uint32_t virtual_address) {
  return virtual_address ? REX_KERNEL_MEMORY()->GetPhysicalAddress(virtual_address) : 0;
}

int ContextIndex(uint32_t context) {
  int index = XmaEngine::Get().IndexOf(context);
  if (index < 0) {
    static std::atomic<bool> warned{false};
    if (!warned.exchange(true)) REXLOG_WARN("[native-xma] call with a foreign context {:08X}", context);
  }
  return index;
}

// Field read/modify on a context while no decode pass runs.
template <typename Fn>
void EditContext(uint32_t context, Fn&& fn) {
  int index = ContextIndex(context);
  if (index < 0) return;
  XmaEngine::Get().Edit(index, [&](uint8_t* p) {
    Ctx c;
    c.Load(p);
    fn(c);
    c.Store(p);
  });
}

uint32_t ReadField(uint8_t* base, uint32_t context, xf::Field f) {
  Ctx c;
  c.Load(rex::memory::GuestPtr(base, context));
  return c[f];
}

#else /* bf2mc madagascar3 doritos */
#endif /* PORT_GAME */
}  // namespace

// ---------------------------------------------------------------------------------------------
#if PORT_GAME == PORT_GAME_STREET3
// XMA* (used by RenderWare Audio)
#else /* bf2mc madagascar3 doritos */
// XMA*
#endif /* PORT_GAME */
// ---------------------------------------------------------------------------------------------

// XMACreateContext(u32* out_context)
REX_HOOK_RAW(__imp__XMACreateContext) {
  uint32_t context = XmaEngine::Get().Allocate();
  Store32(base, ctx.r3.u32, context);
  ctx.r3.u64 = context ? X_STATUS_SUCCESS : X_STATUS_NO_MEMORY;
}

REX_HOOK_RAW(__imp__XMAReleaseContext) {
#if PORT_GAME == PORT_GAME_BF2MC
  TraceCall(ctx.r3.u32, "game: release");
#else /* street3 madagascar3 doritos */
#endif /* PORT_GAME */
  XmaEngine::Get().Release(ctx.r3.u32);
  ctx.r3.u64 = 0;
}

#if PORT_GAME == PORT_GAME_STREET3
// XMAInitializeContext(context, XMA_CONTEXT_INIT*)
REX_HOOK_RAW(__imp__XMAInitializeContext) {
  uint32_t context = ctx.r3.u32, init = ctx.r4.u32;
  int index = ContextIndex(context);
  if (index < 0) {
    ctx.r3.u64 = X_E_FALSE;
    return;
  }
  // XMA_CONTEXT_INIT: in0, in0 packets, in1, in1 packets, read offset, out, out blocks, work buffer,
  // subframe decode count, channel count - 1, sample rate id, loop start/end/count/subframe end/skip.
  uint32_t f[11];
  for (int i = 0; i < 11; i++) f[i] = Load32(base, init + i * 4);
  uint32_t loop_start = Load32(base, init + 44), loop_end = Load32(base, init + 48);
  const uint8_t* loop = rex::memory::GuestPtr(base, init + 52);
  uint8_t loop_count = loop[0], loop_subframe_end = loop[1], loop_subframe_skip = loop[2];
  XmaEngine::Get().Edit(index, [&](uint8_t* p) {
    Ctx c{};
    c.d[xf::kIn0Ptr] = Physical(f[0]);
    c.set(xf::kIn0Packets, f[1]);
    c.d[xf::kIn1Ptr] = Physical(f[2]);
    c.set(xf::kIn1Packets, f[3]);
    c.set(xf::kInReadOffset, f[4]);
    c.d[xf::kOutPtr] = Physical(f[5]);
    c.set(xf::kOutBlocks, f[6]);
    c.set(xf::kSubframeDecodeCount, f[8]);
    c.set(xf::kStereo, f[9] >= 1);
    c.set(xf::kSampleRate, f[10]);
    c.set(xf::kLoopStart, loop_start);
    c.set(xf::kLoopEnd, loop_end);
    c.set(xf::kLoopCount, loop_count);
    c.set(xf::kLoopSubframeEnd, loop_subframe_end);
    c.set(xf::kLoopSubframeSkip, loop_subframe_skip);
    c.Store(p);
  });
  // Initialization also resets the decoder's offsets and validity, as the context clear does.
  XmaEngine::Get().Clear(index);
  ctx.r3.u64 = 0;
}
#elif PORT_GAME == PORT_GAME_BF2MC
// XMAInitializeContext(context, XMA_CONTEXT_INIT*)
REX_HOOK_RAW(__imp__XMAInitializeContext) {
  uint32_t context = ctx.r3.u32, init = ctx.r4.u32;
  int index = ContextIndex(context);
  if (index < 0) {
    ctx.r3.u64 = X_E_FALSE;
    return;
  }
  // XMA_CONTEXT_INIT: in0, in0 packets, in1, in1 packets, read offset, out, out blocks, work buffer,
  // subframe decode count, channel count - 1, sample rate id, loop start/end/count/subframe end/skip.
  uint32_t f[11];
  for (int i = 0; i < 11; i++) f[i] = Load32(base, init + i * 4);
  uint32_t loop_start = Load32(base, init + 44), loop_end = Load32(base, init + 48);
  const uint8_t* loop = rex::memory::GuestPtr(base, init + 52);
  uint8_t loop_count = loop[0], loop_subframe_end = loop[1], loop_subframe_skip = loop[2];
  XmaEngine::Get().Edit(index, [&](uint8_t* p) {
    Ctx c{};
    c.d[xf::kIn0Ptr] = Physical(f[0]);
    c.set(xf::kIn0Packets, f[1]);
    c.d[xf::kIn1Ptr] = Physical(f[2]);
    c.set(xf::kIn1Packets, f[3]);
    c.set(xf::kInReadOffset, f[4]);
    c.d[xf::kOutPtr] = Physical(f[5]);
    c.set(xf::kOutBlocks, f[6]);
    c.set(xf::kSubframeDecodeCount, f[8]);
    c.set(xf::kStereo, f[9] >= 1);
    c.set(xf::kSampleRate, f[10]);
    c.set(xf::kLoopStart, loop_start);
    c.set(xf::kLoopEnd, loop_end);
    c.set(xf::kLoopCount, loop_count);
    c.set(xf::kLoopSubframeEnd, loop_subframe_end);
    c.set(xf::kLoopSubframeSkip, loop_subframe_skip);
    c.Store(p);
  });
  // Initialization also resets the decoder's offsets and validity, as the context clear does.
  XmaEngine::Get().Clear(index);
  XmaEngine::Get().Trace(index, "game: initialize, out blocks", f[6]);
  ctx.r3.u64 = 0;
}
#else /* madagascar3 doritos */
// XMAPlayback uses the context command adapter.
#endif /* PORT_GAME */

#if PORT_GAME == PORT_GAME_STREET3
REX_HOOK_RAW(__imp__XMASetInputBuffer0) {
  uint32_t buffer = Physical(ctx.r4.u32), packets = ctx.r5.u32;
  EditContext(ctx.r3.u32, [&](Ctx& c) {
    c.d[xf::kIn0Ptr] = buffer;
    c.set(xf::kIn0Packets, packets);
  });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMASetInputBuffer1) {
  uint32_t buffer = Physical(ctx.r4.u32), packets = ctx.r5.u32;
  EditContext(ctx.r3.u32, [&](Ctx& c) {
    c.d[xf::kIn1Ptr] = buffer;
    c.set(xf::kIn1Packets, packets);
  });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMAIsInputBuffer0Valid) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kIn0Valid); }
REX_HOOK_RAW(__imp__XMAIsInputBuffer1Valid) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kIn1Valid); }
REX_HOOK_RAW(__imp__XMAIsOutputBufferValid) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kOutValid); }
REX_HOOK_RAW(__imp__XMAGetOutputBufferReadOffset) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kOutRead); }
REX_HOOK_RAW(__imp__XMAGetOutputBufferWriteOffset) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kOutWrite); }

REX_HOOK_RAW(__imp__XMASetInputBuffer0Valid) {
  EditContext(ctx.r3.u32, [](Ctx& c) { c.set(xf::kIn0Valid, 1); });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMASetInputBuffer1Valid) {
  EditContext(ctx.r3.u32, [](Ctx& c) { c.set(xf::kIn1Valid, 1); });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMASetOutputBufferValid) {
  EditContext(ctx.r3.u32, [](Ctx& c) { c.set(xf::kOutValid, 1); });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMASetOutputBufferReadOffset) {
  uint32_t offset = ctx.r4.u32;
  EditContext(ctx.r3.u32, [&](Ctx& c) { c.set(xf::kOutRead, offset); });
  ctx.r3.u64 = 0;
}

// Enabling a context starts a decode pass (the hardware would pick it up asynchronously).
REX_HOOK_RAW(__imp__XMAEnableContext) {
  XmaEngine::Get().Kick(ContextIndex(ctx.r3.u32));
  ctx.r3.u64 = 0;
}

// XMADisableContext(context, wait): no pass may run on the context once this returns.
REX_HOOK_RAW(__imp__XMADisableContext) {
  XmaEngine::Get().Lock(ContextIndex(ctx.r3.u32), true);
  ctx.r3.u64 = X_E_SUCCESS;
}

// ---------------------------------------------------------------------------------------------
#elif PORT_GAME == PORT_GAME_BF2MC
REX_HOOK_RAW(__imp__XMASetInputBuffer0) {
  TraceCall(ctx.r3.u32, "game: set input 0, packets", ctx.r5.u32);
  uint32_t buffer = Physical(ctx.r4.u32), packets = ctx.r5.u32;
  EditContext(ctx.r3.u32, [&](Ctx& c) {
    c.d[xf::kIn0Ptr] = buffer;
    c.set(xf::kIn0Packets, packets);
  });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMASetInputBuffer1) {
  TraceCall(ctx.r3.u32, "game: set input 1, packets", ctx.r5.u32);
  uint32_t buffer = Physical(ctx.r4.u32), packets = ctx.r5.u32;
  EditContext(ctx.r3.u32, [&](Ctx& c) {
    c.d[xf::kIn1Ptr] = buffer;
    c.set(xf::kIn1Packets, packets);
  });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMAIsInputBuffer0Valid) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kIn0Valid); }
REX_HOOK_RAW(__imp__XMAIsInputBuffer1Valid) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kIn1Valid); }
REX_HOOK_RAW(__imp__XMAIsOutputBufferValid) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kOutValid); }
REX_HOOK_RAW(__imp__XMAGetOutputBufferReadOffset) { ctx.r3.u64 = ReadField(base, ctx.r3.u32, xf::kOutRead); }

// Stall detector: the game's streaming mixer (sub_820E9968) busy-waits on the write offset until
// the decoder delivers enough output. Tens of thousands of polls of one unchanged offset within a
// couple of seconds mean it is spinning; the context and its recent history are logged once.
REX_HOOK_RAW(__imp__XMAGetOutputBufferWriteOffset) {
  const uint32_t context = ctx.r3.u32;
  const uint32_t value = ReadField(base, context, xf::kOutWrite);
  ctx.r3.u64 = value;
  struct Watch {
    uint32_t value = UINT32_MAX, polls = 0;
    std::chrono::steady_clock::time_point since;
    bool reported = false;
  };
  static Watch watches[XmaEngine::kContextCount];
  const int index = XmaEngine::Get().IndexOf(context);
  if (index < 0) return;
  Watch& w = watches[index];
  if (value != w.value) {
    if (w.reported)
      REXLOG_WARN("[native-xma] context {} resumed after {:.0f} ms", index,
                  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - w.since).count());
    w = Watch{value, 0, std::chrono::steady_clock::now(), false};
    return;
  }
  if (++w.polls != 2000 || w.reported) return;
  if (std::chrono::steady_clock::now() - w.since > std::chrono::seconds(1)) {
    w.polls = 0;  // slow, legitimate polling of an idle voice
    w.since = std::chrono::steady_clock::now();
    return;
  }
  // A short wait can be input on its way (another thread validating a buffer): give it 20 ms.
  if (std::chrono::steady_clock::now() - w.since < std::chrono::milliseconds(20)) {
    w.polls = 0;
    return;
  }
  // The usual cause: the stream has ended and the game waits for its last frame, which the
  // decoder holds back (XmaEngine::ReleaseHeldFrame). Delivering it lets the game go on; the
  // next poll sees the new write offset.
  const std::string before = XmaEngine::Get().Describe(index);
  if (uint32_t blocks = XmaEngine::Get().ReleaseHeldFrame(index)) {
    static std::atomic<int> logged{0};
    const int n = logged.fetch_add(1);
    if (n < 4)
      REXLOG_INFO("[native-xma] context {}: game waited at its stream end, released {} held blocks; state before:\n{}",
                  index, blocks, before);
    else if (n < 40)
      REXLOG_INFO("[native-xma] context {}: game waited at its stream end, released {} held blocks", index, blocks);
    w.polls = 0;
    w.since = std::chrono::steady_clock::now();
    return;
  }
  w.reported = true;
  XmaEngine::Get().Trace(index, "game: spinning on write offset", value);
  REXLOG_ERROR("[native-xma] game mixer spinning on context {} (decoder made no progress):\n{}", index,
               XmaEngine::Get().Describe(index));
}

REX_HOOK_RAW(__imp__XMASetInputBuffer0Valid) {
  TraceCall(ctx.r3.u32, "game: input 0 valid");
  EditContext(ctx.r3.u32, [](Ctx& c) { c.set(xf::kIn0Valid, 1); });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMASetInputBuffer1Valid) {
  TraceCall(ctx.r3.u32, "game: input 1 valid");
  EditContext(ctx.r3.u32, [](Ctx& c) { c.set(xf::kIn1Valid, 1); });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMASetOutputBufferValid) {
  TraceCall(ctx.r3.u32, "game: output valid");
  EditContext(ctx.r3.u32, [](Ctx& c) { c.set(xf::kOutValid, 1); });
  ctx.r3.u64 = 0;
}

// XMASetInputBufferReadOffset(context, bit offset)
REX_HOOK_RAW(__imp__XMASetInputBufferReadOffset) {
  TraceCall(ctx.r3.u32, "game: set input read bit", ctx.r4.u32);
  uint32_t offset = ctx.r4.u32;
  EditContext(ctx.r3.u32, [&](Ctx& c) { c.set(xf::kInReadOffset, offset); });
  ctx.r3.u64 = 0;
}

// XMASetLoopData(context, XMA_LOOP_DATA*): loop start, loop end (bits), count, subframe end, skip.
REX_HOOK_RAW(__imp__XMASetLoopData) {
  TraceCall(ctx.r3.u32, "game: set loop data");
  uint32_t data = ctx.r4.u32;
  uint32_t loop_start = Load32(base, data), loop_end = Load32(base, data + 4);
  const uint8_t* loop = rex::memory::GuestPtr(base, data + 8);
  uint8_t loop_count = loop[0], loop_subframe_end = loop[1], loop_subframe_skip = loop[2];
  EditContext(ctx.r3.u32, [&](Ctx& c) {
    c.set(xf::kLoopStart, loop_start);
    c.set(xf::kLoopEnd, loop_end);
    c.set(xf::kLoopCount, loop_count);
    c.set(xf::kLoopSubframeEnd, loop_subframe_end);
    c.set(xf::kLoopSubframeSkip, loop_subframe_skip);
  });
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XMASetOutputBufferReadOffset) {
  TraceCall(ctx.r3.u32, "game: set output read", ctx.r4.u32);
  uint32_t offset = ctx.r4.u32;
  EditContext(ctx.r3.u32, [&](Ctx& c) { c.set(xf::kOutRead, offset); });
  ctx.r3.u64 = 0;
}

// Enabling a context starts decoding it (a pass now, then the engine's background sweep).
REX_HOOK_RAW(__imp__XMAEnableContext) {
  TraceCall(ctx.r3.u32, "game: enable");
  XmaEngine::Get().Kick(ContextIndex(ctx.r3.u32));
  ctx.r3.u64 = 0;
}

// XMADisableContext(context, wait): no pass may run on the context once this returns successfully;
// without `wait` it fails while a pass is running.
REX_HOOK_RAW(__imp__XMADisableContext) {
  TraceCall(ctx.r3.u32, "game: disable, wait", ctx.r4.u32);
  bool done = XmaEngine::Get().Disable(ContextIndex(ctx.r3.u32), ctx.r4.u32 != 0);
  ctx.r3.u64 = done ? X_E_SUCCESS : X_E_FALSE;
}

// XMABlockWhileInUse(context): returns once no decode pass is using the context.
REX_HOOK_RAW(__imp__XMABlockWhileInUse) {
  XmaEngine::Get().Lock(ContextIndex(ctx.r3.u32), true);
  ctx.r3.u64 = 0;
}

// ---------------------------------------------------------------------------------------------
#else /* madagascar3 doritos */
#endif /* PORT_GAME */
// XAudio render driver
// ---------------------------------------------------------------------------------------------
#if PORT_GAME == PORT_GAME_STREET3
#elif PORT_GAME == PORT_GAME_BF2MC

// Clients whose voice category volumes were reported (see XAudioGetVoiceCategoryVolumeChangeMask).
static std::atomic<uint32_t> g_volume_reported{0};
#else /* madagascar3 doritos */

#endif /* PORT_GAME */

REX_HOOK_RAW(__imp__XAudioGetSpeakerConfig) {
  Store32(base, ctx.r3.u32, 0x00010001);
  ctx.r3.u64 = 0;
}

// XAudioRegisterRenderDriverClient(u32 callback_and_arg[2], u32* out_driver)
REX_HOOK_RAW(__imp__XAudioRegisterRenderDriverClient) {
  uint32_t callback_ptr = ctx.r3.u32, driver_ptr = ctx.r4.u32;
#if PORT_GAME == PORT_GAME_STREET3
  auto* audio = fs3audio::AudioSystem();
#elif PORT_GAME == PORT_GAME_BF2MC
  auto* audio = bf2audio::AudioSystem();
#else /* madagascar3 doritos */
  auto* audio = m3audio::AudioSystem();
#endif /* PORT_GAME */
  uint32_t callback = callback_ptr ? Load32(base, callback_ptr) : 0;
  if (!audio || !callback) {
    ctx.r3.u64 = X_E_INVALIDARG;
    return;
  }
  uint32_t index = 0;
  X_STATUS status = audio->RegisterClient(callback, Load32(base, callback_ptr + 4), &index);
  if (status != X_STATUS_SUCCESS) {
    ctx.r3.u64 = status;
    return;
  }
  Store32(base, driver_ptr, 0x41550000 | index);
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XAudioUnregisterRenderDriverClient) {
#if PORT_GAME == PORT_GAME_STREET3
  if (auto* audio = fs3audio::AudioSystem()) audio->UnregisterClient(ctx.r3.u32 & 0xFFFF);
#elif PORT_GAME == PORT_GAME_BF2MC
  g_volume_reported &= ~(1u << (ctx.r3.u32 & 31));
  if (auto* audio = bf2audio::AudioSystem()) audio->UnregisterClient(ctx.r3.u32 & 0xFFFF);
#else /* madagascar3 doritos */
  if (auto* audio = m3audio::AudioSystem()) audio->UnregisterClient(ctx.r3.u32 & 0xFFFF);
#endif /* PORT_GAME */
  ctx.r3.u64 = 0;
}

REX_HOOK_RAW(__imp__XAudioSubmitRenderDriverFrame) {
#if PORT_GAME == PORT_GAME_STREET3
  if (auto* audio = fs3audio::AudioSystem()) audio->SubmitFrame(ctx.r3.u32 & 0xFFFF, ctx.r4.u32);
#elif PORT_GAME == PORT_GAME_BF2MC
  if (auto* audio = bf2audio::AudioSystem()) audio->SubmitFrame(ctx.r3.u32 & 0xFFFF, ctx.r4.u32);
#else /* madagascar3 doritos */
  if (auto* audio = m3audio::AudioSystem()) audio->SubmitFrame(ctx.r3.u32 & 0xFFFF, ctx.r4.u32);
#endif /* PORT_GAME */
  ctx.r3.u64 = 0;
}

#if PORT_GAME == PORT_GAME_STREET3
// No system-wide voice category volumes exist on PC: unchanged, full volume.
REX_HOOK_RAW(__imp__XAudioGetVoiceCategoryVolumeChangeMask) {
  std::this_thread::yield();
  Store32(base, ctx.r4.u32, 0);
  ctx.r3.u64 = 0;
}

#elif PORT_GAME == PORT_GAME_BF2MC
// No system-wide voice category volumes exist on PC: full volume, never changed afterwards. The
// game's XAudio (sub_8248E7F8) only reads a category volume (2 categories) when its bit is set in
// the change mask, so the first query of each client reports both as changed to load the 1.0.
REX_HOOK_RAW(__imp__XAudioGetVoiceCategoryVolumeChangeMask) {
  std::this_thread::yield();
  uint32_t client = 1u << (ctx.r3.u32 & 31);
  bool first = !(g_volume_reported.fetch_or(client) & client);
  Store32(base, ctx.r4.u32, first ? 0x3 : 0);
  ctx.r3.u64 = 0;
}

#else /* madagascar3 doritos */
// No system-wide PC category gain: let the game mixer use full scale.
#endif /* PORT_GAME */
REX_HOOK_RAW(__imp__XAudioGetVoiceCategoryVolume) {
  float one = 1.0f;
  uint32_t bits;
  std::memcpy(&bits, &one, 4);
  Store32(base, ctx.r4.u32, bits);
  ctx.r3.u64 = 0;
}

#if PORT_GAME == PORT_GAME_STREET3
// ---------------------------------------------------------------------------------------------
// XMAPlayback (XDK library inside XAudio). It keeps a copy of each hardware context in its stream
// objects and talks to the decoder through registers; these are the only functions that do. The
// stream object: +0 stream count, +4 flags, +8 array of 0x60-byte streams, each with the 48-byte
// context copy at +0, the hardware context pointer at +0x40 and its index at +0x50.
// Flags: 0x10000 locked, 0x20000 copy synchronized from the hardware, 0x40000 contexts allocated.
// ---------------------------------------------------------------------------------------------

namespace {

constexpr uint32_t kStreamSize = 0x60;

struct Streams {
  uint8_t* base;
  uint32_t object;
  uint32_t count() const { return Load32(base, object); }
  uint32_t flags() const { return Load32(base, object + 4); }
  void set_flags(uint32_t v) const { Store32(base, object + 4, v); }
  uint32_t stream(uint32_t i) const { return Load32(base, object + 8) + i * kStreamSize; }
  int index(uint32_t i) const { return int(Load16(base, stream(i) + 0x50)); }
};

// Copies the hardware contexts back into the stream objects once no decode pass is running.
bool SyncFromHardware(const Streams& s) {
  if (s.flags() & 0x20000) return true;
  for (uint32_t i = 0; i < s.count(); i++)
    if (XmaEngine::Get().Busy(s.index(i))) return false;
  uint8_t* base = s.base;
  for (uint32_t i = 0; i < s.count(); i++) {
    uint32_t e = s.stream(i);
    uint32_t hw = Load32(base, e + 0x40);
    XmaEngine::Get().Edit(s.index(i), [&](uint8_t*) { std::memcpy(rex::memory::GuestPtr(base, e), rex::memory::GuestPtr(base, hw), 48); });
    uint32_t d0 = Load32(base, e);
    uint32_t valid = (d0 >> 20) & 3;
    if (!(valid & 1)) {  // input buffer 0 consumed
      Store32(base, e + 20, 0);
      Store32(base, e, d0 & 0xFFFFF000);
      Store32(base, e + 84, 0);
    }
    if (!(valid & 2)) {  // input buffer 1 consumed
      Store32(base, e + 24, 0);
      Store32(base, e + 4, Load32(base, e + 4) & 0xFFFFF000);
      Store32(base, e + 88, 0);
    }
    if (!valid) Store32(base, e + 16, Load32(base, e + 16) & 0x7FFFFFFF);
  }
  s.set_flags(s.flags() | 0x20000);
  return true;
}

}  // namespace

// Reads the context array address (decoder register) into the library's global.
REX_HOOK_RAW(sub_824D5040) {
  (void)ctx;
  Store32(base, 0x8267A064, XmaEngine::Get().array_physical());
}

// Allocates and clears a hardware context for every stream that has none.
REX_HOOK_RAW(sub_824D50B0) {
  Streams s{base, ctx.r3.u32};
  for (uint32_t i = 0; i < s.count(); i++) {
    uint32_t e = s.stream(i);
    if (Load32(base, e + 0x40)) continue;
    uint32_t context = XmaEngine::Get().Allocate();
    if (!context) {
      ctx.r3.u64 = X_STATUS_NO_MEMORY;
      return;
    }
    Store32(base, e + 0x40, context);
    int index = XmaEngine::Get().IndexOf(context);
    Store16(base, e + 0x50, uint16_t(index));
    XmaEngine::Get().Clear(index);
  }
  s.set_flags(s.flags() | 0x40000);
  ctx.r3.u64 = 0;
}

// Locks (disables) every stream's context.
REX_HOOK_RAW(sub_824D5330) {
  Streams s{base, ctx.r3.u32};
  if ((s.flags() & 0x30000) == 0)
    for (uint32_t i = 0; i < s.count(); i++) XmaEngine::Get().Lock(s.index(i), true);
  s.set_flags(s.flags() | 0x10000);
  ctx.r3.u64 = 0;
}

// Synchronizes the stream copies from the hardware; 0 while a context is still being processed.
REX_HOOK_RAW(sub_824D53B0) {
  ctx.r3.u64 = SyncFromHardware(Streams{base, ctx.r3.u32}) ? 1 : 0;
}

// Writes the stream copies to the hardware contexts and kicks them.
REX_HOOK_RAW(sub_824D5500) {
  Streams s{base, ctx.r3.u32};
  if ((s.flags() & 0x20000) && s.count()) {
    for (uint32_t i = 0; i < s.count(); i++) {
      uint32_t e = s.stream(i);
      uint32_t hw = Load32(base, e + 0x40);
      XmaEngine::Get().Edit(s.index(i), [&](uint8_t*) { std::memcpy(rex::memory::GuestPtr(base, hw), rex::memory::GuestPtr(base, e), 48); });
    }
  }
  for (uint32_t i = 0; i < s.count(); i++) XmaEngine::Get().Kick(s.index(i));
  s.set_flags(s.flags() & ~0x30000u);
  ctx.r3.u64 = 0;
}

// Waits for the streams to go idle; after ~125 ms resets them instead (returns 1 in that case).
REX_HOOK_RAW(sub_824D5DA0) {
  Streams s{base, ctx.r3.u32};
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(125);
  while (!SyncFromHardware(s)) {
    if (std::chrono::steady_clock::now() > deadline) {
      for (uint32_t i = 0; i < s.count(); i++) {
        XmaEngine::Get().Lock(s.index(i), true);
        XmaEngine::Get().Clear(s.index(i));
      }
      SyncFromHardware(s);
      for (uint32_t i = 0; i < s.count(); i++) {
        rex::CallFrame frame(ctx);
        frame.ctx.r3.u64 = s.object;
        frame.ctx.r4.u64 = i;
        sub_824D5990(frame, base);
      }
      s.set_flags(s.flags() | 0x80000);
      ctx.r3.u64 = 1;
      return;
    }
    std::this_thread::yield();
  }
  ctx.r3.u64 = 0;
}

#else /* bf2mc madagascar3 doritos */
#endif /* PORT_GAME */
#pragma once
#include "port_game.h"
#include <bit>
#include <cstdint>
#include "../pc_settings.h"

#if PORT_GAME == PORT_GAME_COPA2014
// Post-processing switches (same policy as Street 4). Copa's final brightness/contrast pass is
// the same pixel shader as Street 4's (identical microcode hash). Its vignette lives in a
// different composite shader that was not identified, so it has no switch.
#else /* street4 wc2010 */
// Engine-independent policy. Copa's port should reuse the constant rules after
// identifying its shader hashes, not copy Street 4's guest addresses.
#endif /* PORT_GAME */
namespace ea_postfx {
#if PORT_GAME == PORT_GAME_COPA2014
enum Effect { BrightnessContrast, Count };
enum class Pass { Other, BrightnessContrast };
#else /* street4 wc2010 */
enum Effect { Vignette, BrightnessContrast, Count };
enum class Pass { Other, Vignette, BrightnessContrast };
#endif /* PORT_GAME */
inline Pass Identify(uint64_t pixel_shader_hash) {
#if PORT_GAME == PORT_GAME_COPA2014
  return pixel_shader_hash == 0xa392657905ecd8c0ull ? Pass::BrightnessContrast : Pass::Other;
#else /* street4 wc2010 */
  switch (pixel_shader_hash) {
    case 0xba02458e2167f262ull: return Pass::Vignette;
    case 0xa392657905ecd8c0ull: return Pass::BrightnessContrast;
    default: return Pass::Other;
  }
#endif /* PORT_GAME */
}
inline bool Enabled(Pass pass) {
#if PORT_GAME == PORT_GAME_COPA2014
  return pass != Pass::BrightnessContrast || pcs::EffectEnabled(BrightnessContrast);
#else /* street4 wc2010 */
  switch (pass) {
    case Pass::Vignette: return pcs::EffectEnabled(Vignette);
    case Pass::BrightnessContrast: return pcs::EffectEnabled(BrightnessContrast);
    default: return true;
  }
#endif /* PORT_GAME */
}
#if PORT_GAME == PORT_GAME_COPA2014
// Host-endian PS float constants, after literal overlay. Keep the original draw: it copies the
// current scene into its output; skipping it would leave stale data.
#else /* street4 wc2010 */
// Host-endian PS float constants, after literal overlay. Keep the original draw:
// it copies the current scene into its output; skipping it would leave stale data.
#endif /* PORT_GAME */
inline void Neutralize(Pass pass, uint32_t* ps) {
#if PORT_GAME == PORT_GAME_COPA2014
  if (pass == Pass::BrightnessContrast) {
#else /* street4 wc2010 */
  if (pass == Pass::Vignette) {
    // c1.w mixes scene RGB towards vignette colour. Zero = unchanged scene.
    ps[1 * 4 + 3] = 0;
  } else if (pass == Pass::BrightnessContrast) {
#endif /* PORT_GAME */
    // Output = scene * c0.y + c0.x. Identity preserves the alpha/depth contract.
    ps[0] = 0;
    ps[1] = std::bit_cast<uint32_t>(1.0f);
  }
}
}

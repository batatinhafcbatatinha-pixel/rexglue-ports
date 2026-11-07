#pragma once
#include "port_game.h"
#include "../platform/native_shader_compiler.h"

#if PORT_GAME == PORT_GAME_BF2MC
namespace bf2n {
#else /* madagascar3 doritos */
namespace m3gpu {
#endif /* PORT_GAME */
// The translator explicitly implements Xenos NaN/Inf and zero-multiply rules.
// FXC otherwise folds isnan(input) to false, even at optimization level 1.
constexpr UINT kShaderCompileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL1 |
                                     D3DCOMPILE_IEEE_STRICTNESS;
}

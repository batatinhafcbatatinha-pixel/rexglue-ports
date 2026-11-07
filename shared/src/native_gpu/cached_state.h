#pragma once
#include "port_game.h"
#include <cstring>
#include <type_traits>

#if PORT_GAME == PORT_GAME_BF2MC
namespace bf2n {
#else /* madagascar3 doritos */
namespace m3gpu {
#endif /* PORT_GAME */
// Compare exact representations, including floating-point signed zero and NaN payloads.
// Reset when a command list ends or an uncached utility pass changes the context.
template <typename T> struct CachedState {
  static_assert(std::is_trivially_copyable_v<T>);
  T value{};
  bool valid = false;
  bool Changed(const T& next) {
    if (valid && std::memcmp(&value, &next, sizeof(T)) == 0) return false;
    std::memcpy(&value, &next, sizeof(T));
    valid = true;
    return true;
  }
};
}

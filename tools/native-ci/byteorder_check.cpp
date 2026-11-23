#include "native_byteorder.h"
#include <cstdint>

int main() {
    using namespace native_host;
    if (ByteSwap16(0x1234) != 0x3412) return 1;
    if (ByteSwap32(0x12345678) != 0x78563412) return 2;
    if (ByteSwap64(UINT64_C(0x0123456789abcdef)) != UINT64_C(0xefcdab8967452301)) return 3;
    if (ByteSwap64(ByteSwap64(UINT64_C(0x0123456789abcdef))) != UINT64_C(0x0123456789abcdef)) return 4;
    return 0;
}

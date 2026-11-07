#pragma once

namespace rex::memory { class Memory; }

namespace m3audio {
// Compatibility boundary for this XEX's XMAPlayback command writes. Decoding is
// done by the native FFmpeg engine, not the SDK's XmaDecoder/APU implementation.
bool InstallXmaCommands(rex::memory::Memory* memory);
}

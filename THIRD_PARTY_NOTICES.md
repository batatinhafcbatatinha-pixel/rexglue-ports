# Third-party notices

The code of this repository is our own and is offered under the MIT license ([LICENSE](LICENSE)). This file
records what the shared code under `shared/` owes to other projects. Each game has its own `NOTICE.md` and
`licenses/` folder for the dependencies its build downloads (ReXGlue SDK, FFmpeg headers, DXVK, fonts).

## Xenia / ReXGlue SDK (BSD-3-Clause)

License text: [licenses/ReXGlue-Xenia-LICENSE.txt](licenses/ReXGlue-Xenia-LICENSE.txt).

No Xenia source file is copied into this repository. The following files were written here and follow the
data layouts and semantics documented by Xenia's headers, which ship with the ReXGlue SDK:

| Our files | What they follow |
| --- | --- |
| `shared/src/native/shader_translator.cpp`, `shared/src/native_gpu/xenos_hlsl.cpp` | Xenos shader microcode instruction encodings (`rex/graphics/format/ucode.h`) |
| `shared/src/native/texture_loader.{h,cpp}`, `shared/src/native_gpu/textures.h` | texture tiling, mip-tail and pitch rules (`texture_util`, `texture_address.h`), and the texture-format sign table of the texture cache |
| `shared/src/native/native_audio.cpp`, `shared/src/native_audio/xma_engine.cpp` | XMA context layout and processing order (`apu/xma_context`) |
| `shared/src/native/renderer.cpp` | the behaviour of one compatibility option (invalid fetch constants) |

If a file of another project is ever copied or adapted here, it is listed in this file with its origin,
revision and license before it is committed.

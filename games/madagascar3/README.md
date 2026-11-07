# Madagascar 3: The Video Game — Native PC Port (work in progress) | No Emulated GPU or Audio Hardware

> [!NOTE]
> This port now lives in the [rexglue-ports](../../) repository as `games/madagascar3`. The native
> layers it shares with the other Xbox 360 ports (renderer, XMA audio, PC settings menu, platform code) are in
> [`shared/src`](../../shared/src/); this game keeps its own code and a `project/src/port_game.h`. Clone
> rexglue-ports (with `git submodule update --init` for the game-file installer) and run the scripts from
> this folder.

> [!WARNING]
> This repository does **not** include the game, an ISO/XEX, game assets, generated game code or a precompiled executable.
> You need your own Xbox 360 USA disc. See [the full notice](#notice) at the end.

An experimental port of the Xbox 360 game *DreamWorks Madagascar 3: The Video Game*, built through static recompilation with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), with its own Direct3D 11 renderer and its own XMA audio pipeline, like the [FIFA Street 3](../street3/) and [Battlefield 2: Modern Combat](../bf2mc/) ports. **This public repository is a work-in-progress source and local-build kit, not a ready-to-play game download.** The game's PowerPC code is translated locally from your own disc; no game executable or generated game code is distributed here.

## How to play

This port is built from **your own disc**. Clone [rexglue-ports](https://github.com/batatinhafcbatatinha-pixel/rexglue-ports) with its submodules
(`git clone --recursive https://github.com/batatinhafcbatatinha-pixel/rexglue-ports.git`), then in `games/madagascar3` run `Setup.cmd`: it installs the build
tools, asks for your disc image or extracted files, checks them and builds the port. Then play with the
`Play-*.cmd` in that folder.

**Problems?** Open an [issue](https://github.com/batatinhafcbatatinha-pixel/rexglue-ports/issues) naming the game, with `setup-log.txt` and a screenshot.
Never attach game files.

## Status

- Reaches gameplay (Monte Carlo level, characters, HUD, minimap and dialogue) on Windows 11 x64 at 30 fps, as on the console, without the SDK's Xenos GPU plugin. At 4K with 8x MSAA it measured 26 fps in the 3D hub (RTX 3050 Laptop).
- Graphics: the game's D3D calls are drawn by a native Direct3D 11 renderer; Xenos shader microcode is translated to HLSL.
- Audio: the game's XMA exports are reimplemented on the host (FFmpeg's XMA decoder, SDL3 output).
- **F1** (or Back + Start) opens the PC settings, drawn in the style of the game's own Options menu: display mode, internal resolution, VSync, FXAA, MSAA (off, 2x, 4x or 8x; applies at once), anisotropic filtering up to 16×, DirectX 11 or Vulkan through DXVK, volume.
- Known issues: a hang if the language is confirmed within about 3 seconds of the screen appearing; a rare fatal call to address 0; MSAA does not apply to the deferred G-buffer. Saves, full progression and stability are unverified.

## Controls

### Controller

Keyboard and mouse: WASD moves, the arrows turn the camera, Space jumps, left and right mouse buttons are X and B, E is Y, Escape is START, Tab is BACK (`teclado=0` in the ini turns this off).

Xbox pads and every pad SDL knows work in the game and in the PC settings menu: DualShock 3/4, DualSense, Switch pads, USB adapters for PS2 and original Xbox pads. `tools/fetch_pad_support.py` downloads `gamecontrollerdb.txt` (SDL_GameControllerDB, zlib license) next to the executable, so generic adapters get their button names. Only Xbox-type input was tested; no PlayStation pad or adapter has been tried on real hardware yet. The pad vibrates when the game asks for it; **Controller vibration** in the PC settings turns that off.

## Your game files

```text
default.xex  38d0d7e6665c89567b0566dff387d2925a0b7f001367b99fe08bfd48e32fd1a0
```

Do not upload the ISO, extracted files, generated code or the built executable.

## Building by hand

`Setup.cmd` ([How to play](#how-to-play)) does all of this on Windows. The steps below are for doing it yourself, or on Linux.

### Windows

Requirements: Python 3.10+, Visual Studio 2022 Build Tools (C++ workload, Windows SDK), LLVM/Clang 20 or newer (a complete install, with `lld-link`), CMake 3.25+ and Ninja.

```powershell
python tools/build_local.py --game-files game_files --llvm-dir "C:\Program Files\LLVM"
python tools/play.py --game-files game_files
```

`build_local.py` validates your files, downloads the pinned ReXGlue SDK nightly (`0.10.0.24-dev.gbd833a2`, SHA-256 checked) with its small FPSCR patch, the FFmpeg headers matching the SDK's `libavcodec` and DXVK, translates your `default.xex` into private C++ under `.local/` and compiles the port. `--reuse-generated` skips the translation on later builds. The author's own pipeline (`build-madagascar.cmd`, `scripts/pipeline.py`) expects a shared workspace and is not needed.

### Linux

Linux (x86-64, native, no Wine): the same `tools/build_local.py` takes `--dxvk-native-dir` (DXVK native 3.1.1: the D3D11 renderer on Vulkan) and `--vkd3d-dir` (vkd3d-shader 1.19+, the HLSL compiler), with Clang 20, CMake, Ninja, pkg-config, fontconfig and FreeType development files. It puts the SDK runtime, DXVK native and vkd3d-shader next to the executable. Not built for Linux yet with this game.

```sh
python3 tools/build_local.py --game-files game_files \
  --dxvk-native-dir /path/to/dxvk-native/usr --vkd3d-dir /path/to/vkd3d-install
python3 tools/play.py --game-files game_files
```

### macOS

Experimental and not compiled or run yet: see [MACOS.md](MACOS.md).

## Why a native port instead of an emulator

An emulator such as [Xenia](https://xenia.jp) recreates the Xbox 360: it JIT-translates the PowerPC code at run time and emulates the Xenos GPU by processing the console's command buffers, and the audio hardware by modelling its XMA contexts. That is the right approach for the whole library. This port takes a different route for one game:

- **Ahead-of-time code.** The game's PowerPC code becomes C++ compiled by a modern optimizing compiler before you play: no JIT, no warm-up.
- **No GPU emulation in the hot path.** Draw calls go from the game's D3D library straight to Direct3D 11, instead of being turned into command packets and register writes that are then emulated again.
- **Native audio.** XMA streams are decoded and mixed directly on the PC.
- **Readable and fixable.** Every hook, renderer path and audio routine is plain C++ in `project/src`.

The trade-off: it covers only this game and revision, and it is experimental. An emulator is the more general and better-tested option.

## What is in this repository

- `project/` — CMake project, ReXGlue manifest (which game functions are translated; `game_root` points to your `game_files`), and the native code in `project/src/` (renderer, shader translation, audio, PC settings, scripted input).
- `tools/` — `build_local.py` (downloads the pinned ReXGlue SDK, FFmpeg headers and DXVK, translates your executable, compiles), `play.py`, `validate_game.py`, `fetch_pad_support.py`, `setup.ps1` / `setup_tools.py` (behind `Setup.cmd`), `audit_publication.py` and the shared game-file installer in `tools/port-installer`.
- `scripts/` — ISO inspection/extraction, image summary, import-call scan, function-extent and indirect-entry recovery, function matching, PowerPC disassembler, the author's build pipeline and test runner.
- `inputs/` — scripted-input files for automated test runs.
- `kit.json`, `installer.json`, `setup.json`, `licenses/`, [NOTICE.md](NOTICE.md) and [NOTES.md](NOTES.md) (development log, in Portuguese).

## Credits and licenses

- Port integration, native renderer and audio: [batatinhafcbatatinha-pixel](https://github.com/batatinhafcbatatinha-pixel).
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk): Tom Clay and contributors, BSD-3-Clause; derived from [Xenia](https://github.com/xenia-project/xenia) (Ben Vanik and contributors). Shader-microcode semantics and XMA field layouts follow the SDK's Xenia-derived headers.
- [PT Sans](https://github.com/google/fonts/tree/main/ofl/ptsans) by ParaType (SIL Open Font License 1.1, `project/fonts/OFL.txt`): the game's own typeface, shipped for the PC settings menu.
- [FFmpeg](https://ffmpeg.org) (LGPL-2.1) for XMA decoding, [SDL3](https://www.libsdl.org) (zlib) for audio output, [DXVK](https://github.com/doitsujin/dxvk) (zlib) as the optional Vulkan path; external dependencies, not bundled.
- Madagascar 3: The Video Game, its code, trademarks and assets belong to DreamWorks Animation, D3Publisher, Monkey Bar Games and the respective rights holders. This project is not affiliated with or endorsed by them or by Microsoft.

---

<a id="notice"></a>

**THIS PROJECT DOES NOT INCLUDE THE GAME, AN ISO/XEX, GAME ASSETS, GENERATED GAME CODE OR A PRECOMPILED GAME EXECUTABLE. YOU MUST PROVIDE YOUR OWN LAWFULLY OBTAINED XBOX 360 GAME FILES AND HAVE THE RIGHTS REQUIRED FOR YOUR USE. THIS PROJECT DOES NOT AUTHORIZE COPYRIGHT INFRINGEMENT. ALL GAME RIGHTS BELONG TO THEIR RESPECTIVE OWNERS.**

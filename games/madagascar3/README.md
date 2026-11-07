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

An experimental port of the Xbox 360 game *DreamWorks Madagascar 3: The Video Game*, built through static recompilation with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), with its own Direct3D 11 renderer and its own XMA audio pipeline, like the [FIFA Street 3](https://github.com/batatinhafcbatatinha-pixel/fifa-street-3-pc) and [Battlefield 2: Modern Combat](https://github.com/batatinhafcbatatinha-pixel/bf2-modern-combat-pc) ports. **This public repository is a work-in-progress source and local-build kit, not a ready-to-play game download.** The game's PowerPC code is translated locally from your own disc; no game executable or generated game code is distributed here.

## How to play

> [!IMPORTANT]
> **There is no ready-to-play download.** This page has only tools: they turn *your own* game disc into a PC game,
> on your computer. The first time takes a while (20-60 minutes on a recent PC), mostly waiting.
> After that, you just double-click to play.

**You need**

- **Windows 10 or 11** (64-bit) with about **19 GB** free. (Linux and Mac: see below.)
- Your own **Madagascar 3: The Video Game** (Xbox 360), as a disc image (`.iso`) made from your disc, or its extracted
  files. Other versions are not accepted; the setup checks this for you.

**Steps**

**Easiest: `Setup.exe`.** Open [Releases](https://github.com/batatinhafcbatatinha-pixel/madagascar-3-pc/releases), download **`Setup.exe`** from the newest one and run it.
It downloads this kit (source code only), shows what your PC already has and what is missing on its
*Dependencies* tab, installs what is missing, asks for your `.iso` and builds the game. Afterwards the same
program (and its desktop shortcut) is your launcher: **Play**, and **PC settings** (the same options as F1 in
the game, before it starts). Windows may say it protected your PC because the program is not signed: click
*More info* > *Run anyway*. The program is built by GitHub from [tools/launcher](tools/launcher); nothing from the game is in it.

**Or step by step:**

1. **Download**, one of these two ways:
   <details open><summary><b>Easiest: the ZIP</b></summary>

   Open [Releases](https://github.com/batatinhafcbatatinha-pixel/madagascar-3-pc/releases/latest), download **Source code (zip)** and extract it to a short folder, such as
   `C:\Games\madagascar-3-pc`.
   </details>
   <details><summary><b>With Git: always the newest version</b></summary>

   ```powershell
   git clone --recursive https://github.com/batatinhafcbatatinha-pixel/madagascar-3-pc.git C:\Games\madagascar-3-pc
   ```
   To update later: `git pull --recurse-submodules`, then run `Setup.cmd` again.
   </details>

2. **Double-click `Setup.cmd`.** It asks before installing anything, then:
   - installs Python and Microsoft's C++ Build Tools if you don't have them (free; Windows asks for permission once);
   - downloads the other build tools (about 1 GB) **into this folder only**;
   - opens a window where you **pick your `.iso`**: it checks the version and copies the game files;
   - builds your PC version, and offers a desktop shortcut.

   If anything stops, run `Setup.cmd` again: finished steps are skipped.

3. **Play:** double-click **`Play-Madagascar3.cmd`** (or the desktop shortcut).

**In the game:** **F1** opens the PC settings. Controllers and the keyboard work: see [Controls](#controls).

**Problems?** Open an [issue](https://github.com/batatinhafcbatatinha-pixel/madagascar-3-pc/issues) with `setup-log.txt` and a screenshot. Never attach game files.

`Setup.cmd` is new: so far it has been tried from a fresh download only with the FIFA Street 3 port. If it stops for you, please open an issue with `setup-log.txt`.

<details><summary><b>Linux and Mac</b></summary>

- **Linux:** works natively (no Wine), but the setup is by hand for now: see [Linux](#linux).
- **Mac:** not playable yet. The Mac build files are an experiment that has never been compiled or run on a Mac. Details in [MACOS.md](MACOS.md); if you have a Mac and want to help test, open an issue.

</details>

<details><summary><b>Questions</b></summary>

- **Is this legal?** This page contains no game, only tools. You use the game you already own.
- **Can I share my built game?** No: it contains the game's code. Keep it on your PC.
- **Will it run on my PC?** Any graphics card with DirectX 11 should work. Measured: 30 fps (the game's own rate) on a Ryzen 5 3600 + RTX 3050 Laptop.
- **How do I remove it?** Delete the folder. Python and the Build Tools can be removed in Windows Settings > Apps.

</details>

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

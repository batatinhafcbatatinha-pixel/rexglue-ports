# rexglue-ports

Native PC ports of Xbox 360 games, built from **your own disc** on top of the
[ReXGlue](https://github.com/rexglue) static recompiler (PowerPC to C++). No game content is stored here: no disc
image, executable, game data or generated code.

What the ports add on top of the recompiled code is native: a Direct3D 11 renderer (also on Vulkan through DXVK,
DXVK native on Linux, DXMT on macOS), XMA audio decoded on the host, the in-game PC settings menu (F1) and
modern keyboard, mouse and controller input. The SDK's GPU emulation is not used.

## Layout

| Folder | What it holds |
| --- | --- |
| [`games/`](games/) | one folder per game: its manifest, hooks, game-specific native code, build and play scripts |
| [`shared/src/`](shared/src/) | the native layers the games share (see below) |
| `tools/port-installer` | the game-file installer / launcher (submodule: `git submodule update --init`) |

Shared layers (`shared/src`):

- `platform/` - window, shader compiler, Win32 subset for Linux/macOS, fonts
- `pc_settings.*`, `pc_font.*`, `script_input.h` - the PC settings menu (each game keeps its own `pc_theme.cpp` look)
- `native_audio/` - XMA decoding and audio output (FIFA Street 3, BF2: Modern Combat, Madagascar 3, Doritos)
- `native_gpu/` - the Xenos renderer of Madagascar 3, BF2: Modern Combat and Doritos
- `native/` - the renderer and audio of the FIFA Street 2012 engine family (FIFA Street, 2014 FIFA World Cup, 2010 FIFA World Cup)

Each game's `project/src/port_game.h` sets `PORT_GAME` (ids in `shared/src/port_games.h`) and the guest addresses
the shared code needs; behaviour that differs between games is selected with `#if PORT_GAME == PORT_GAME_<NAME>`.
The shared files were merged from the games' former copies and checked so that every game compiles the same code
as before.

## Games

| Game | Disc | State |
| --- | --- | --- |
| [FIFA Street 3](games/street3/) | Xbox 360 | playable |
| [FIFA Street (2012)](games/street4/) | Xbox 360 | playable |
| [Battlefield 2: Modern Combat](games/bf2mc/) | Xbox 360 | playable |
| [Madagascar 3](games/madagascar3/) | Xbox 360 | reaches gameplay |
| [2014 FIFA World Cup Brazil](games/copa2014/) | Xbox 360 | in progress |
| [Doritos Crash Course](games/doritos/) | Xbox 360 (XBLA) | early work |
| [2010 FIFA World Cup South Africa](games/wc2010/) | Xbox 360 | early work |

Each game folder has its own README with how to build and play it (`Setup.cmd` where present).

## License

The ports' own code and tooling: MIT ([LICENSE](LICENSE)); third-party notices in each game's `NOTICE.md` and
`licenses/`. The games, their code, trademarks and assets belong to their publishers and rights holders; these
ports are not affiliated with or endorsed by them or by Microsoft.

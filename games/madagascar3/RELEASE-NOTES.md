Source code and `Setup.exe` (our installer and launcher): no game, no XEX, no generated code, no game executable. Bring your own Xbox 360 disc.

**v0.1.5**

- **`Setup.exe`**: download it alone and run it. It fetches this repository's source (the release's source ZIP), shows on its *Dependencies* tab what your PC has and what is missing (Python, C++ Build Tools, LLVM/CMake/Ninja, your game files, the built game) with the space each part needs, installs each part or everything, and builds the game. Afterwards it is the launcher: *Play*, and *PC settings* with the same options as the F1 menu, saved before the game starts. Dark and light themes (the default follows Windows). It is built by GitHub from `tools/launcher`; nothing from the game is in it. Windows may warn because it is not signed.
- `Setup.cmd` still works as before; `tools/setup.ps1` gained `-Only python|buildtools|tools|game`.

`Setup.exe` was tested with the FIFA Street 3 kit (stand-in install steps, Play, settings file, real kit download up to the build tools); for this game only its window and settings were checked, not a build through it.


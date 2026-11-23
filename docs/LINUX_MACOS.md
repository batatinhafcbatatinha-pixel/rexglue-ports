# Linux and macOS

Experimental source support, untested. No Linux/macOS builds or runs were performed for this change.
Windows entry points are retained. Nothing here requires Wine.

The five folders containing `kit.json` have launcher overrides, `build-linux.sh`,
`build-macos.sh` and `Play.sh`. They reuse the existing verified SDK and disc checks.
Linux uses DXVK native on Vulkan; Intel macOS uses DXMT on Metal. Both use
vkd3d-shader 1.19 or later, SDL3, FreeType and fontconfig. The pinned SDK is x86-64.
Apple Silicon native builds are unsupported; the new entry point refuses Rosetta.

Install Python 3.11+, Clang 20 or newer, CMake 3.31.12, Ninja 1.13.2 and pkg-config.
The launcher lists distro packages for apt, dnf, pacman and zypper, or Homebrew on macOS.
Package availability varies by distribution release; older releases need newer Clang and SDL3 repositories.
On macOS use Xcode and an Intel Homebrew LLVM installation. The SDK's C++ ABI must match the compiler.

Graphics dependencies are not downloaded automatically. Supply existing native installations:

```sh
export DXVK_NATIVE_DIR=/path/to/dxvk-native
export VKD3D_DIR=/path/to/vkd3d
# On an Intel Mac, use this instead of DXVK_NATIVE_DIR:
export DXMT_DIR=/path/to/dxmt
```

Defaults are `.local/tools/native/linux/{dxvk,vkd3d}` or
`.local/tools/native/macos/{dxmt,vkd3d}` at the repository root. DXVK native must
provide `include/dxvk` and `lib` or `lib64`, built with SDL3 WSI. DXMT must provide
`include/windows`, `include/directx`, `lib/d3d11.dylib` and `lib/dxgi.dylib`.
The vkd3d prefix must have `lib/pkgconfig` (or `lib64/pkgconfig`).
Use the SDK's SDL3 ABI; mixing independent SDL instances can break window lookup.
The older per-profile macOS dependency scripts use Git downloads without SHA-256
verification and are not invoked by the new entry point. Replacing those with
fully pinned archives, including their transitive dependencies, is still open.

From a profile folder, after installing your own disc files into `game_files`:

```sh
bash build-linux.sh --jobs 2
# On an Intel Mac:
bash build-macos.sh --jobs 2
bash Play.sh
```

`--game-files /path/to/disc` overrides the disc folder. `--reuse-generated` reuses
completed translation. Portable CMake/Ninja are discovered in `.local/tools`, as
in the launcher; `PORT_TOOLS_DIR` overrides that directory. Compiler discovery uses
PATH and the existing profile compiler selection. Generated code and executables
stay in the profile's `.local` folder. Do not publish those files.

Open limitations:

- The two folders without `kit.json` lack a verified portable packaging pipeline.
  Their new scripts stop with an explicit error; they are not advertised as playable.
- The generic path uses the SDK's GPU emulation plugin. Its Windows-only low-memory
  fault recovery in `tools/x360port/x360port_host.cpp` also has no POSIX equivalent.
  Linux/macOS launcher configuration therefore disables generic builds under the
  native-only requirement. Tool discovery and SDK executable permissions are fixed
  independently, but a native generic renderer remains unimplemented.
- Full renderer, audio, Win32-subset and macOS loader/Metal ABI compilation remain
  unverified. Native dependencies must already exist; installation is not turnkey.
- CI on ubuntu-latest and macos-15 compiles the standalone shader translator for
  each profile and checks byte order, Python, shell and JSON. It uses no disc files
  or generated game sources. It does not build the SDK or full runtime libraries.
  The optional `PORTS_CHECK_HOST_BRIDGE=ON` CMake option builds the host bridge
  when its graphics dependencies are provisioned. ARM CI covers portable sources,
  not the x86-64 SDK or full application.

No new third-party dependency is introduced. Existing dependency notices still apply.

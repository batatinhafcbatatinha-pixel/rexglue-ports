#include "port_game.h"
#include "../platform/native_byteorder.h"
// Offline Xenos shader -> host Shader Model 5 compiler. Does not launch a game.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifndef _WIN32
#include "../platform/native_win32_posix.h"
#endif
#include "../platform/native_shader_compiler.h"
#include "../platform/native_com_ptr.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "xenos_hlsl.h"

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

int wmain(int argc, wchar_t** argv) {
  if (argc != 4 || (std::wstring_view(argv[1]) != L"vs" &&
                    std::wstring_view(argv[1]) != L"ps")) {
#if PORT_GAME == PORT_GAME_DORITOS
    std::cerr << "Usage: doritos_shaderc vs|ps <big-endian-microcode> <new-output-directory>\n";
#else /* madagascar3 */
    std::cerr << "Usage: madagascar3_shaderc vs|ps <big-endian-microcode> <new-output-directory>\n";
#endif /* PORT_GAME */
    return 2;
  }
  try {
    const bool pixel = std::wstring_view(argv[1]) == L"ps";
    const fs::path input(argv[2]), output(argv[3]);
    if (!fs::is_regular_file(input) || fs::exists(output)) {
      std::cerr << "Input must be a file; output directory must be new.\n";
      return 2;
    }
    const auto size = fs::file_size(input);
    if (!size || size % 4 || size > 1024 * 1024) {
      std::cerr << "Microcode must have 1..1048576 bytes, aligned to dwords.\n";
      return 2;
    }
    fs::path probe = fs::absolute(output).parent_path();
    while (!fs::exists(probe)) probe = probe.parent_path();
    if (fs::space(probe).available < 20ull * 1024 * 1024 * 1024) {
      std::cerr << "20 GiB free disk reserve required.\n";
      return 2;
    }
    std::vector<uint32_t> code(size / 4);
    std::ifstream source(input, std::ios::binary);
    if (!source.read(reinterpret_cast<char*>(code.data()), size)) return 3;
    for (auto& word : code) word = native_host::ByteSwap32(word);
    m3gpu::ShaderInfo info;
    m3gpu::TranslateOptions options;
    const std::string hlsl = m3gpu::TranslateXenosShader(code.data(), uint32_t(code.size()), pixel, options, info);
    if (!info.ok || info.uses_memexport) {
      std::cerr << "Unsupported shader: " << info.error
                << (info.uses_memexport ? "; native memexport integration missing" : "") << '\n';
      return 4;
    }
    ComPtr<ID3DBlob> bytecode, diagnostics;
    const char* profile = pixel ? "ps_5_0" : "vs_5_0";
    const HRESULT status = D3DCompile(hlsl.data(), hlsl.size(), "xenos-offline", nullptr, nullptr,
        "main", profile, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0, &bytecode, &diagnostics);
    if (diagnostics) std::cerr.write(static_cast<const char*>(diagnostics->GetBufferPointer()), diagnostics->GetBufferSize());
    if (FAILED(status)) return 5;
    fs::create_directories(output);
    std::ofstream hlsl_file(output / "shader.hlsl", std::ios::binary);
    hlsl_file.write(hlsl.data(), hlsl.size());
    std::ofstream dxbc_file(output / "shader.dxbc", std::ios::binary);
    dxbc_file.write(static_cast<const char*>(bytecode->GetBufferPointer()), bytecode->GetBufferSize());
    std::ofstream metadata(output / "shader.json");
    metadata << "{\n  \"schemaVersion\": 1,\n  \"profile\": \"" << profile
             << "\",\n  \"sourceBytes\": " << size
             << ",\n  \"dxbcBytes\": " << bytecode->GetBufferSize()
             << ",\n  \"textureBindings\": " << info.textures.size()
             << ",\n  \"writesDepth\": " << (info.writes_depth ? "true" : "false")
             << ",\n  \"usesKill\": " << (info.uses_kill ? "true" : "false")
             << ",\n  \"hostDrawIntegrationComplete\": false\n}\n";
    if (!hlsl_file || !dxbc_file || !metadata) return 6;
    std::cout << "Compiled " << profile << " into " << bytecode->GetBufferSize() << " bytes of DXBC.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 7;
  }
}

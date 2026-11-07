cmake_minimum_required(VERSION 3.25)
project(doritoscrashcourse LANGUAGES CXX)
# Native layers shared by the ports (../../../shared).
set(PORTS_SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../../../shared" CACHE PATH
    "Native layers shared by the ports (tools/build_local.py passes it when it builds a copy of project/)")
# This game's own sources first (port_game.h and the headers the shared files include).
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src ${PORTS_SHARED}/src ${PORTS_SHARED}/src/native_audio ${PORTS_SHARED}/src/native_gpu ${PORTS_SHARED}/src/platform)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

add_executable(doritoscrashcourse WIN32
    src/main.cpp
    src/hooks.cpp
    src/debug_peek.cpp
    src/avatar_xam.cpp
    ${PORTS_SHARED}/src/native_audio/audio_exports.cpp
    ${PORTS_SHARED}/src/native_audio/audio_system.cpp
    ${PORTS_SHARED}/src/native_audio/xma_engine.cpp
    ${PORTS_SHARED}/src/native_audio/xma_commands.cpp
    ${PORTS_SHARED}/src/native_gpu/gpu_hooks.cpp
    ${PORTS_SHARED}/src/pc_settings.cpp
    src/pc_theme.cpp
    ${PORTS_SHARED}/src/pc_font.cpp
)

rexglue_setup_target(doritoscrashcourse)
# PDB with public symbols (sub_XXXXXXXX names) for crash stacks.
if(WIN32)
    target_link_options(doritoscrashcourse PRIVATE -Xlinker /DEBUG)
endif()

# Native audio (Madagascar 3 port): FFmpeg XMA decoder from the SDK; public headers shared with Battlefield 2.
target_link_libraries(doritoscrashcourse PRIVATE rex::libavcodec rex::libavutil)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/include/libavcodec/avcodec.h")
    target_include_directories(doritoscrashcourse PRIVATE third_party/ffmpeg/include)
else()
    target_include_directories(doritoscrashcourse PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../battlefield2/project/third_party/ffmpeg/include")
endif()

# Native D3D11 renderer (Madagascar 3's, hooks matched in ${PORTS_SHARED}/src/native_gpu/gpu_hooks.cpp).
add_library(doritos_native_shaders STATIC ${PORTS_SHARED}/src/native_gpu/xenos_hlsl.cpp)
target_include_directories(doritos_native_shaders PUBLIC src/native_gpu)
add_library(doritos_native_gpu STATIC ${PORTS_SHARED}/src/native_gpu/renderer.cpp ${PORTS_SHARED}/src/native_gpu/textures.cpp)
target_include_directories(doritos_native_gpu PRIVATE src/native_gpu)
target_link_libraries(doritos_native_gpu PUBLIC doritos_native_shaders rex::runtime rex::xxhash)
if(WIN32)
    target_link_libraries(doritos_native_gpu PUBLIC d3d11 dxgi d3dcompiler winmm)
endif()
target_link_libraries(doritoscrashcourse PRIVATE doritos_native_gpu)

# Settings menu font files.
add_custom_command(TARGET doritoscrashcourse POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:doritoscrashcourse>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
            "$<TARGET_FILE_DIR:doritoscrashcourse>/fonts")

# Vulkan option: DXVK next to the executable in vulkan\.
set(DXVK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/dxvk-3.1.1" CACHE PATH "DXVK release for Windows")
if(WIN32 AND EXISTS "${DXVK_DIR}/x64/d3d11.dll")
    add_custom_command(TARGET doritoscrashcourse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:doritoscrashcourse>/vulkan"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXVK_DIR}/x64/d3d11.dll" "${DXVK_DIR}/x64/dxgi.dll"
                "$<TARGET_FILE_DIR:doritoscrashcourse>/vulkan/")
endif()

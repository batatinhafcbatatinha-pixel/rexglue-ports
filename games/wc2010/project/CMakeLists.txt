# wc2010 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(wc2010 LANGUAGES CXX)
# Native layers shared by the ports (../../../shared).
set(PORTS_SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../../../shared")
# This game's own sources first (port_game.h and the headers the shared files include).
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/native ${PORTS_SHARED}/src ${PORTS_SHARED}/src/native ${PORTS_SHARED}/src/platform)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Launcher: native graphics system (no GPU plugin) and PC options.
set(WC2010_SOURCES
    src/main.cpp
    ${PORTS_SHARED}/src/pc_settings.cpp
    src/pc_theme.cpp
    ${PORTS_SHARED}/src/pc_font.cpp
)

if(WIN32)
    add_executable(wc2010 WIN32 ${WC2010_SOURCES})
else()
    add_executable(wc2010 ${WC2010_SOURCES})
endif()
target_include_directories(wc2010 PRIVATE src)

# No GPU_PLUGINS: the Xenos emulation plugin is intentionally not staged.
rexglue_setup_target(wc2010)

# Hooks on functions of the game module must be compiled into that module.
if(TARGET wc2010_fifadllzf)
    target_sources(wc2010_fifadllzf PRIVATE
        src/kernel_extra.cpp
        src/input_hooks.cpp
        ${PORTS_SHARED}/src/native/d3d_hooks.cpp
        ${PORTS_SHARED}/src/native/renderer.cpp
        ${PORTS_SHARED}/src/native/shader_translator.cpp
        ${PORTS_SHARED}/src/native/texture_loader.cpp
        ${PORTS_SHARED}/src/native/native_audio.cpp
        ${PORTS_SHARED}/src/native/game_log.cpp
        src/native/game_natives.cpp
        ${PORTS_SHARED}/src/pc_settings.cpp
        src/pc_theme.cpp
        ${PORTS_SHARED}/src/pc_font.cpp
    )
    target_include_directories(wc2010_fifadllzf PRIVATE src src/native)
    # XMA decoding with the SDK's prebuilt libavcodec (Xenia FFmpeg fork); headers from
    # the matching commit in third_party/ffmpeg.
    if(WIN32)
        target_include_directories(wc2010_fifadllzf PRIVATE ../third_party/ffmpeg)
        find_library(WC2010_AVCODEC NAMES libavcodecrd libavcodec PATHS ${CMAKE_PREFIX_PATH}/lib NO_DEFAULT_PATH)
        find_library(WC2010_AVUTIL NAMES libavutilrd libavutil PATHS ${CMAKE_PREFIX_PATH}/lib NO_DEFAULT_PATH)
        target_link_libraries(wc2010_fifadllzf PRIVATE ${WC2010_AVCODEC} ${WC2010_AVUTIL})
    else()
        # The same Xenia FFmpeg fork as on Windows (the system FFmpeg has no XMA decoder).
        target_include_directories(wc2010_fifadllzf PRIVATE ../third_party/ffmpeg)
        find_package(SDL3 REQUIRED CONFIG)
        target_link_libraries(wc2010_fifadllzf PRIVATE rex::libavcodec rex::libavutil SDL3::SDL3)
    endif()
    if(WIN32)
        target_link_libraries(wc2010_fifadllzf PRIVATE d3d11 dxgi xaudio2 ole32 bcrypt)
    endif()
endif()


# Offline shader translator check (tool; never runs the game).
if(WIN32)
    add_executable(wc2010_shader_test tools/shader_test.cpp ${PORTS_SHARED}/src/native/shader_translator.cpp)
    target_include_directories(wc2010_shader_test PRIVATE src ${CMAKE_PREFIX_PATH}/include)
    target_compile_definitions(wc2010_shader_test PRIVATE NOMINMAX)
endif()

# Launcher dispatch table relocation (see src/wc2010_layout.h): appended to the
# recompiled launcher's precompiled header so it is processed after the generated one.
if(TARGET wc2010_recomp)
    target_precompile_headers(wc2010_recomp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wc2010_layout.h")
endif()
if(TARGET wc2010_fifadllzf)
    target_precompile_headers(wc2010_fifadllzf PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/wc2010_dll_dispatch.h")
endif()

# Vulkan option of the settings menu: DXVK's d3d11.dll/dxgi.dll in vulkan\ next to the
# executable (pc_settings.cpp loads them by full path when "API grafica" is Vulkan).
set(DXVK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/dxvk-3.1.1" CACHE PATH
    "DXVK release for Windows (x64/d3d11.dll, x64/dxgi.dll, LICENSE)")
if(WIN32 AND EXISTS "${DXVK_DIR}/x64/d3d11.dll")
    add_custom_command(TARGET wc2010 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:wc2010>/vulkan"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXVK_DIR}/x64/d3d11.dll" "${DXVK_DIR}/x64/dxgi.dll"
                "$<TARGET_FILE_DIR:wc2010>/vulkan/")
    if(EXISTS "${DXVK_DIR}/LICENSE")
        add_custom_command(TARGET wc2010 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXVK_DIR}/LICENSE"
                    "$<TARGET_FILE_DIR:wc2010>/vulkan/LICENSE.dxvk.txt")
    endif()
endif()

# Native HLSL compiler for Linux (no Windows compiler DLL).
include(${PORTS_SHARED}/src/platform/native_shader_compiler.cmake)
if(TARGET native_shader_compiler)
    foreach(_native_target IN ITEMS wc2010 wc2010_fifadllzf)
        if(TARGET ${_native_target})
            target_link_libraries(${_native_target} PRIVATE native_shader_compiler)
        endif()
    endforeach()
endif()

cmake_minimum_required(VERSION 3.25)
project(native_source_checks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(REPO "${CMAKE_CURRENT_LIST_DIR}/../..")
enable_testing()
add_executable(byteorder_check byteorder_check.cpp)
target_include_directories(byteorder_check PRIVATE "${REPO}/shared/src/platform")
add_test(NAME byteorder COMMAND byteorder_check)

# Compile every existing profile's variant without generated code or disc data.
file(GLOB profiles LIST_DIRECTORIES true "${REPO}/games/*")
set(profile_index 0)
foreach(profile IN LISTS profiles)
    if(EXISTS "${profile}/project/src/port_game.h")
        math(EXPR profile_index "${profile_index} + 1")
        add_library(shader_translation_${profile_index} STATIC
            "${REPO}/shared/src/native_gpu/xenos_hlsl.cpp")
        target_include_directories(shader_translation_${profile_index} PRIVATE
            "${profile}/project/src" "${REPO}/shared/src")
    endif()
endforeach()

# Optional full host bridge build for machines with provisioned native dependencies.
option(PORTS_CHECK_HOST_BRIDGE "Build the native host bridge with installed graphics libraries" OFF)
if(PORTS_CHECK_HOST_BRIDGE)
    include("${REPO}/shared/src/platform/native_shader_compiler.cmake")
endif()

// Doritos Crash Course for the in-game PC settings (pc_settings.h).
#pragma once

#include "pc_settings.h"

const pcs::Theme* PcDoritosTheme();  // pc_theme.cpp

// Keyboard as the controller: WASD/arrows to run and steer, Space to jump (A), C/RMB for B.
inline constexpr const char* kKeybinds[][2] = {
    {"keybind_lstick_up", "W,Up"},       {"keybind_lstick_down", "S,Down"},
    {"keybind_lstick_left", "A,Left"},   {"keybind_lstick_right", "D,Right"},
    {"keybind_rstick_up", "I"},          {"keybind_rstick_down", "K"},
    {"keybind_rstick_left", "J"},        {"keybind_rstick_right", "L"},
    {"keybind_a", "Space,Return"},       {"keybind_b", "C,RMB,Backspace"},
    {"keybind_x", "LMB,F"},              {"keybind_y", "E"},
    {"keybind_left_trigger", "Q"},       {"keybind_right_trigger", "R"},
    {"keybind_left_shoulder", "1"},      {"keybind_right_shoulder", "2"},
    {"keybind_lstick_press", "V"},       {"keybind_rstick_press", "B"},
    {"keybind_dpad_up", "T"},            {"keybind_dpad_down", "G"},
    {"keybind_dpad_left", "Z"},          {"keybind_dpad_right", "X"},
    {"keybind_start", "Escape,Return"},  {"keybind_back", "Tab"},
    {"keybind_guide", ""}};

inline pcs::GameInfo PcGameInfo() {
  pcs::GameInfo info;
  info.keybinds = kKeybinds;
  info.keybind_count = int(sizeof(kKeybinds) / sizeof(kKeybinds[0]));
  info.title = L"DORITOS CRASH COURSE";
  info.window_title = "Doritos Crash Course";
  info.ini_name = L"doritos-pc.ini";
  info.internal_resolution = true;
  info.msaa = true;
  // Flips are paced to the game's own present interval (gpu_hooks.cpp).
  info.real_time = false;
  info.theme = PcDoritosTheme();
  return info;
}

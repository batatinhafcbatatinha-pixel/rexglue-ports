// DreamWorks Madagascar 3 for the in-game PC settings (pc_settings.h).
#pragma once

#include "pc_settings.h"

const pcs::Theme* PcMadagascarTheme();  // pc_theme.cpp

// Keyboard and mouse as the controller: WASD to move, the arrows for the camera, Space to jump,
// the mouse buttons for the two attacks. Every other SDK default would reuse keys.
inline constexpr const char* kKeybinds[][2] = {
    {"keybind_lstick_up", "W"},          {"keybind_lstick_down", "S"},
    {"keybind_lstick_left", "A"},        {"keybind_lstick_right", "D"},
    {"keybind_rstick_up", "Up"},         {"keybind_rstick_down", "Down"},
    {"keybind_rstick_left", "Left"},     {"keybind_rstick_right", "Right"},
    {"keybind_a", "Space,Return"},       {"keybind_b", "RMB,Backspace"},
    {"keybind_x", "LMB,J"},              {"keybind_y", "E,K"},
    {"keybind_left_trigger", "Q"},       {"keybind_right_trigger", "R"},
    {"keybind_left_shoulder", "1"},      {"keybind_right_shoulder", "2"},
    {"keybind_lstick_press", "C"},       {"keybind_rstick_press", "V"},
    {"keybind_dpad_up", "I"},            {"keybind_dpad_down", "M"},
    {"keybind_dpad_left", "H"},          {"keybind_dpad_right", "L"},
    {"keybind_start", "Escape,Return"},  {"keybind_back", "Tab"},
    {"keybind_guide", ""}};

inline pcs::GameInfo PcGameInfo() {
  pcs::GameInfo info;
  info.keybinds = kKeybinds;
  info.keybind_count = int(sizeof(kKeybinds) / sizeof(kKeybinds[0]));
  info.title = L"MADAGASCAR 3";
  info.window_title = "Madagascar 3: The Video Game";
  info.ini_name = L"madagascar3-pc.ini";
  // The game asks for its language itself at the first boot (seven languages).
  info.internal_resolution = true;
  info.msaa = true;
  // One fixed step per frame, presented every second vblank (gpu_hooks.cpp paces the flips):
  // more frames would speed the game up.
  info.real_time = false;
  info.theme = PcMadagascarTheme();
  return info;
}

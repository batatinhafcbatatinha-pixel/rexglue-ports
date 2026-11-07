// 2010 FIFA World Cup South Africa for the in-game PC settings (pc_settings.h). Used by the launcher and
// by the game module, which each keep a copy of the settings state.
#pragma once

#include "pc_settings.h"
#include "native/post_effects.h"

const pcs::Theme* PcWc2010Theme();  // pc_theme.cpp

inline pcs::GameInfo PcGameInfo() {
  // data/loc/locale.big ships eng_us and spa_es (XC_LANGUAGE ids).
  static const pcs::Language kLanguages[] = {{1, L"English"}, {5, L"Español"}};
  pcs::GameInfo info;
  info.title = L"2010 FIFA WORLD CUP";
  info.window_title = "2010 FIFA World Cup";
  info.ini_name = L"wc2010-pc.ini";
  info.languages = kLanguages;
  info.language_count = int(sizeof(kLanguages) / sizeof(kLanguages[0]));
  info.internal_resolution = true;
  info.msaa = true;
  static const pcs::Effect kEffects[] = {
      {"vignette", L"Vinheta", L"Vignette",
       L"Escurecimento das bordas da imagem.", L"Darkening around the edges of the image."},
      {"brightness_contrast", L"Brilho e contraste cinematográficos", L"Cinematic brightness and contrast",
       L"Ajuste de brilho e contraste aplicado pelo jogo.", L"The game's final brightness and contrast adjustment."},
  };
  static_assert(sizeof(kEffects) / sizeof(kEffects[0]) == ea_postfx::Count);
  info.effects = kEffects;
  info.effect_count = ea_postfx::Count;
  info.football_keyboard = true;
  // Measured 2026-10-20 at a 120 Hz vblank with no limit: the match/drill clock keeps real time at
  // 70-150 fps and the menus do not wait for the vblank, so frame rates above 60 are safe.
  info.real_time = true;
  info.theme = PcWc2010Theme();
  return info;
}

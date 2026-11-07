#include "platform/native_host_window.h"
// madagascar3 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>
#include <string>
#include <rex/rex_app.h>
#include <rex/input/input_system.h>
#include "script_input.h"
#include "pc_game_info.h"
#include "native_audio/audio_system.h"
#include "native_gpu/native_gpu.h"

class Madagascar3App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Madagascar3App>(new Madagascar3App(ctx, "madagascar3",
        PPCImageConfig));
  }

 protected:
  void OnPreSetup(rex::RuntimeConfig& config) override {
    // In-game PC settings (F1 / Back+Start): window mode, vsync, FXAA, filtering, resolution, API.
    pcs::Configure(PcGameInfo());
    config.graphics = nullptr;
    config.gpu_plugin.clear();
    config.audio_factory = [](rex::runtime::FunctionDispatcher* dispatcher)
        -> std::unique_ptr<rex::system::IAudioSystem> {
      return std::make_unique<m3audio::NativeAudioSystem>(dispatcher);
    };
    // MADAGASCAR3_INPUT=<file>: scripted pad (see script_input.h) next to the real ones.
    if (const char* script = std::getenv("MADAGASCAR3_INPUT"); script && *script) {
      config.input_factory = [path = std::string(script)](bool tool_mode)
          -> std::unique_ptr<rex::system::IInputSystem> {
        auto input = rex::input::CreateDefaultInputSystem(tool_mode);
        auto driver = std::make_unique<ScriptInputDriver>(path);
        driver->Setup();
        input->AddDriver(std::move(driver));
        return input;
      };
    }
    pcs::InstallPadTap(config);
  }
  void OnPostSetup() override { m3gpu::NativeGpuInstallMmio(); }
  void OnPreLaunchModule() override {
    m3gpu::NativeGpuSetWindow(window() ? native_host::WindowHandle(window()) : nullptr);
  }
  void OnShutdown() override { m3gpu::NativeGpuShutdown(); }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};

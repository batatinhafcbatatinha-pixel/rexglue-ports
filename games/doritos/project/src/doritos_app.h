#pragma once

#include <cstdlib>
#include <string>

#include "generated/default/doritoscrashcourse_init.h"

#include <rex/input/device_assignment.h>
#include <rex/input/input_system.h>
#include <rex/rex_app.h>

#include "native_audio/audio_system.h"
#include "native_gpu/native_gpu.h"
#include "pc_game_info.h"
#include "platform/native_host_window.h"
#include "script_input.h"

namespace doritos { void StartPeekThread(); }

// Test runs: user 0 = script pad only, so a real pad's stick noise cannot take the slot over.
class ScriptOnlyAssignment final : public rex::input::DeviceAssignment {
 public:
  void OnDevicesChanged(const std::vector<rex::input::DeviceInfo>& devices) override {
    script_.clear();
    for (const auto& d : devices)
      if (d.guid == "script") script_.push_back(d.id);
  }
  void DevicesForUser(uint32_t user_index, std::vector<rex::input::DeviceId>& out) const override {
    if (user_index == 0) out.insert(out.end(), script_.begin(), script_.end());
  }
 private:
  std::vector<rex::input::DeviceId> script_;
};

class DoritosCrashCourseApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& context) {
    return std::unique_ptr<DoritosCrashCourseApp>(
        new DoritosCrashCourseApp(context, "Doritos Crash Course", PPCImageConfig));
  }

 protected:
  void OnPostSetup() override {
    m3gpu::NativeGpuInstallMmio();
    doritos::StartPeekThread();
  }
  void OnPreLaunchModule() override {
    m3gpu::NativeGpuSetWindow(window() ? native_host::WindowHandle(window()) : nullptr);
  }
  void OnShutdown() override { m3gpu::NativeGpuShutdown(); }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    // In-game PC settings (F1 / Back+Start) and the native D3D11 renderer (native_gpu/, from Madagascar 3).
    pcs::Configure(PcGameInfo());
    config.graphics = nullptr;
    config.gpu_plugin.clear();
    // Native audio (render driver + FFmpeg XMA) instead of the SDK APU, whose worker stopped servicing the
    // game's two render-driver clients after the first frames (almost silent).
    if (!std::getenv("DORITOS_SDK_AUDIO"))
      config.audio_factory = [](rex::runtime::FunctionDispatcher* dispatcher)
          -> std::unique_ptr<rex::system::IAudioSystem> {
        return std::make_unique<m3audio::NativeAudioSystem>(dispatcher);
      };
    // DORITOS_INPUT=<file>: scripted pad (see script_input.h) next to the real ones.
    if (const char* script = std::getenv("DORITOS_INPUT"); script && *script) {
      config.input_factory = [path = std::string(script)](bool tool_mode)
          -> std::unique_ptr<rex::system::IInputSystem> {
        auto input = rex::input::CreateDefaultInputSystem(tool_mode);
        auto driver = std::make_unique<ScriptInputDriver>(path);
        driver->Setup();
        input->AddDriver(std::move(driver));
        if (!std::getenv("DORITOS_INPUT_WITH_PADS"))
          input->SetDeviceAssignment(std::make_unique<ScriptOnlyAssignment>());
        return input;
      };
    }
    pcs::InstallPadTap(config);
  }
};

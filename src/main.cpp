#include "main.hpp"
#include "VivifyRuntime.hpp"
#include <string_view>
#include "HMUI/ViewController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "bsml/shared/BSML-Lite/Creation/Layout.hpp"
#include "bsml/shared/BSML-Lite/Creation/Settings.hpp"
#include "bsml/shared/BSML/Settings/BSMLSettings.hpp"
#include "custom-types/shared/register.hpp"
#include "scotland2/shared/modloader.h"

static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

namespace {
constexpr std::string_view kMultipassRenderingConfigKey = "multipassRendering";
constexpr std::string_view kVivifyDebugLoggingConfigKey = "vivifyDebugLogging";
constexpr std::string_view kDisableBeat0FilmgrainBlitConfigKey = "disableBeat0FilmgrainBlit";
constexpr std::string_view kDisableAllBlitsConfigKey = "disableAllBlits";
constexpr std::string_view kDisableCreateCameraDepthConfigKey = "disableCreateCameraDepth";
constexpr std::string_view kDisableVRCenterAdjustConfigKey = "disableVRCenterAdjust";
constexpr std::string_view kDisableCustomNoteVisualsConfigKey = "disableCustomNoteVisuals";
constexpr std::string_view kDisableVisualsInMultiplayerConfigKey = "disableVisualsInMultiplayer";
bool gMultipassRenderingEnabled = true;
bool gVivifyDebugLogging = false;
bool gDisableBeat0FilmgrainBlit = false;
bool gDisableAllBlits = false;
bool gDisableCreateCameraDepth = false;
bool gDisableVRCenterAdjust = false;
bool gDisableCustomNoteVisuals = false;
bool gDisableVisualsInMultiplayer = true;

void EnsureConfigObject() {
  auto& doc = getConfig().config;
  if (!doc.IsObject()) {
    doc.SetObject();
  }
}

bool EnsureBoolConfigValue(std::string_view key, bool defaultValue, bool& value) {
  auto& doc = getConfig().config;
  auto it = doc.FindMember(key.data());
  if (it != doc.MemberEnd() && it->value.IsBool()) {
    value = it->value.GetBool();
    return false;
  }

  auto& allocator = doc.GetAllocator();
  value = defaultValue;
  if (it == doc.MemberEnd()) {
    doc.AddMember(rapidjson::Value(key.data(), allocator), rapidjson::Value(defaultValue), allocator);
  } else {
    it->value.SetBool(defaultValue);
  }
  return true;
}

void SetBoolConfigValue(std::string_view key, bool enabled, bool& value) {
  auto& config = getConfig();
  auto& doc = config.config;
  EnsureConfigObject();
  auto& allocator = doc.GetAllocator();
  auto it = doc.FindMember(key.data());
  bool needsWrite = true;
  if (it == doc.MemberEnd()) {
    doc.AddMember(rapidjson::Value(key.data(), allocator), rapidjson::Value(enabled), allocator);
  } else if (it->value.IsBool() && it->value.GetBool() == enabled) {
    needsWrite = false;
  } else {
    it->value.SetBool(enabled);
  }
  value = enabled;
  if (needsWrite) {
    config.Write();
  }
}

void SetVivifyDebugLogging(bool enabled) {
  SetBoolConfigValue(kVivifyDebugLoggingConfigKey, enabled, gVivifyDebugLogging);
}

void SetDisableBeat0FilmgrainBlit(bool enabled) {
  SetBoolConfigValue(kDisableBeat0FilmgrainBlitConfigKey, enabled, gDisableBeat0FilmgrainBlit);
  Vivify::RefreshIsolationSettings();
}

void SetDisableAllBlits(bool enabled) {
  SetBoolConfigValue(kDisableAllBlitsConfigKey, enabled, gDisableAllBlits);
  Vivify::RefreshIsolationSettings();
}

void SetDisableCreateCameraDepth(bool enabled) {
  SetBoolConfigValue(kDisableCreateCameraDepthConfigKey, enabled, gDisableCreateCameraDepth);
  Vivify::RefreshIsolationSettings();
}

bool GetDisableCustomNoteVisuals() {
  // Force-enable custom note visuals on Quest forks.
  return false;
}

void SetDisableCustomNoteVisuals(bool enabled) {
  SetBoolConfigValue(kDisableCustomNoteVisualsConfigKey, enabled, gDisableCustomNoteVisuals);
}

void SetDisableVRCenterAdjust(bool enabled) {
  SetBoolConfigValue(kDisableVRCenterAdjustConfigKey, enabled, gDisableVRCenterAdjust);
  Vivify::RefreshIsolationSettings();
}

bool GetDisableVisualsInMultiplayer() {
  return gDisableVisualsInMultiplayer;
}

void SetDisableVisualsInMultiplayer(bool enabled) {
  SetBoolConfigValue(kDisableVisualsInMultiplayerConfigKey, enabled, gDisableVisualsInMultiplayer);
}

void RegisterModSettings() {
  BSML::BSMLSettings::get_instance()->TryAddSettingsMenu(
      [](HMUI::ViewController* viewController, bool firstActivation, bool, bool) {
        if (!firstActivation || viewController == nullptr) return;
        auto* container = BSML::Lite::CreateScrollableSettingsContainer(viewController->get_transform());
        if (container == nullptr) return;
        BSML::Lite::CreateToggle(container->get_transform(), u"Multipass Rendering", GetMultipassRenderingEnabled(),
                                 [](bool value) {
                                   SetMultipassRenderingEnabled(value);
                                   Vivify::RefreshMultipassRendering();
                                 });
        BSML::Lite::CreateToggle(container->get_transform(), u"Vivify Debug Logging", GetVivifyDebugLogging(),
                                 [](bool value) { SetVivifyDebugLogging(value); });
        BSML::Lite::CreateToggle(container->get_transform(), u"Disable Beat 0 Filmgrain Blit",
                                 GetDisableBeat0FilmgrainBlit(),
                                 [](bool value) { SetDisableBeat0FilmgrainBlit(value); });
        BSML::Lite::CreateToggle(container->get_transform(), u"Disable All Blits", GetDisableAllBlits(),
                                 [](bool value) { SetDisableAllBlits(value); });
        BSML::Lite::CreateToggle(container->get_transform(), u"Disable Custom Vivify Note Visuals",
                                 GetDisableCustomNoteVisuals(),
                                 [](bool value) { SetDisableCustomNoteVisuals(value); });
        BSML::Lite::CreateToggle(container->get_transform(), u"Disable Vivify Visuals In Multiplayer",
               GetDisableVisualsInMultiplayer(),
               [](bool value) { SetDisableVisualsInMultiplayer(value); });
        BSML::Lite::CreateToggle(container->get_transform(), u"Disable CreateCamera/Depth",
                                 GetDisableCreateCameraDepth(),
                                 [](bool value) { SetDisableCreateCameraDepth(value); });
        BSML::Lite::CreateToggle(container->get_transform(), u"Disable VR Center Adjust",
                                 GetDisableVRCenterAdjust(),
                                 [](bool value) { SetDisableVRCenterAdjust(value); });
      },
      "Vivify", false);
}
}

Configuration &getConfig() {
  static Configuration config(modInfo);
  return config;
}

bool GetMultipassRenderingEnabled() {
  return gMultipassRenderingEnabled;
}

bool GetVivifyDebugLogging() {
  return gVivifyDebugLogging;
}

bool GetDisableBeat0FilmgrainBlit() {
  return gDisableBeat0FilmgrainBlit;
}

bool GetDisableAllBlits() {
  return gDisableAllBlits;
}

bool GetDisableCreateCameraDepth() {
  return gDisableCreateCameraDepth;
}

bool GetDisableVRCenterAdjust() {
  return gDisableVRCenterAdjust;
}

void SetMultipassRenderingEnabled(bool enabled) {
  SetBoolConfigValue(kMultipassRenderingConfigKey, enabled, gMultipassRenderingEnabled);
}

void EnsureConfigDefaults() {
  auto& config = getConfig();
  auto& doc = config.config;
  EnsureConfigObject();
  bool needsWrite = false;
  needsWrite |= EnsureBoolConfigValue(kMultipassRenderingConfigKey, true, gMultipassRenderingEnabled);
  needsWrite |= EnsureBoolConfigValue(kVivifyDebugLoggingConfigKey, false, gVivifyDebugLogging);
  needsWrite |= EnsureBoolConfigValue(kDisableBeat0FilmgrainBlitConfigKey, false, gDisableBeat0FilmgrainBlit);
  needsWrite |= EnsureBoolConfigValue(kDisableAllBlitsConfigKey, false, gDisableAllBlits);
  needsWrite |= EnsureBoolConfigValue(kDisableCustomNoteVisualsConfigKey, false, gDisableCustomNoteVisuals);
  needsWrite |= EnsureBoolConfigValue(kDisableVisualsInMultiplayerConfigKey, true, gDisableVisualsInMultiplayer);
  needsWrite |= EnsureBoolConfigValue(kDisableCreateCameraDepthConfigKey, false, gDisableCreateCameraDepth);
  needsWrite |= EnsureBoolConfigValue(kDisableVRCenterAdjustConfigKey, false, gDisableVRCenterAdjust);
  if (needsWrite) {
    config.Write();
  }
}

bool GetDisableCustomNoteVisuals() {
  // Force-enable custom note visuals on Quest forks.
  return false;
}

bool GetDisableVisualsInMultiplayer() {
  return gDisableVisualsInMultiplayer;
}

MOD_EXTERN_FUNC void setup(CModInfo *info) noexcept {
  *info = modInfo.to_c();
  getConfig().Load();
  EnsureConfigDefaults();
}
MOD_EXTERN_FUNC void late_load() noexcept {
  il2cpp_functions::Init();
  custom_types::Register::AutoRegister();
  RegisterModSettings();
  Vivify::LateLoad();
}

#include "VivifyRuntime.hpp"
#include "main.hpp"
#include "VivifyHandlers.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <filesystem>
#include <fstream>
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/AlwaysVisibleQuad.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/BpmController.hpp"
#include "GlobalNamespace/StaticBeatmapObjectSpawnMovementData.hpp"
#include "UnityEngine/Animator.hpp"
#include "UnityEngine/AssetBundle.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/Component.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/DepthTextureMode.hpp"
#include "UnityEngine/FilterMode.hpp"
#include "UnityEngine/FogMode.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Graphics.hpp"
#include "UnityEngine/Light.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/MaterialPropertyBlock.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/QualitySettings.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/RenderSettings.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/RenderTextureDescriptor.hpp"
#include "UnityEngine/RenderTextureFormat.hpp"
#include "UnityEngine/Rendering/AmbientMode.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/SystemInfo.hpp"
#include "UnityEngine/Texture.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Vector4.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Rigidbody.hpp"
#include "UnityEngine/Collider.hpp"
#include "UnityEngine/Video/VideoPlayer.hpp"
#include "UnityEngine/XR/XRSettings.hpp"
#include "GlobalNamespace/SaberModelController.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/BladeMovementDataElement.hpp"
#include "GlobalNamespace/ColorManager.hpp"
#include "GlobalNamespace/IBladeMovementData.hpp"
#include "GlobalNamespace/SaberTrail.hpp"
#include "GlobalNamespace/SaberTrailRenderer.hpp"
#include "GlobalNamespace/SaberType.hpp"
#include "GlobalNamespace/SaberTypeObject.hpp"
#include "GlobalNamespace/ColorType.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteControllerBase.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"
#include "GlobalNamespace/BoxCuttableBySaber.hpp"
#include "GlobalNamespace/CuttableBySaber.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/NoteCutCoreEffectsSpawner.hpp"
#include "GlobalNamespace/NoteCutDirection.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteDebris.hpp"
#include "GlobalNamespace/NoteVisualModifierType.hpp"
#include "GlobalNamespace/SphereCuttableBySaber.hpp"
#include "GlobalNamespace/MaterialPropertyBlockController.hpp"
#include "GlobalNamespace/NoteSpawnData.hpp"
#include "GlobalNamespace/GamePause.hpp"
#include "GlobalNamespace/LayerMasks.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/VRCenterAdjust.hpp"
#include "System/Object.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "custom-json-data/shared/CustomBeatmapData.h"
#include "custom-json-data/shared/CustomEventData.h"
#include "custom-types/shared/macros.hpp"
#include "paper2_scotland2/shared/logger.hpp"
#include "scotland2/shared/modloader.h"
#include "songcore/shared/Capabilities.hpp"
#include "songcore/shared/SongCore.hpp"
#include "tracks/shared/Animation/Easings.h"
#include "tracks/shared/Animation/GameObjectTrackController.hpp"
#include "tracks/shared/Animation/PointDefinition.h"
#include "tracks/shared/Animation/TransformData.hpp"
#include "tracks/shared/AssociatedData.h"
#include "tracks/shared/Constants.h"
#include "tracks/shared/StaticHolders.hpp"
#include "web-utils/shared/WebUtils.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "metacore/shared/game.hpp"
#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
using namespace std::string_view_literals;
DECLARE_CLASS_CODEGEN_INTERFACES(Vivify, OffsetBladeMovementData, System::Object, GlobalNamespace::IBladeMovementData*) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Init, GlobalNamespace::IBladeMovementData* followed, UnityEngine::Transform* parent,
                          UnityEngine::Vector3 topPos, UnityEngine::Vector3 bottomPos);
  DECLARE_OVERRIDE_METHOD_MATCH(float_t, get_bladeSpeed, &GlobalNamespace::IBladeMovementData::get_bladeSpeed);
  DECLARE_OVERRIDE_METHOD_MATCH(GlobalNamespace::BladeMovementDataElement, get_lastAddedData,
                                &GlobalNamespace::IBladeMovementData::get_lastAddedData);
  DECLARE_OVERRIDE_METHOD_MATCH(GlobalNamespace::BladeMovementDataElement, get_prevAddedData,
                                &GlobalNamespace::IBladeMovementData::get_prevAddedData);

private:
  GlobalNamespace::IBladeMovementData* _followed = nullptr;
  UnityEngine::Transform* _parent = nullptr;
  UnityEngine::Vector3 _topPos = UnityEngine::Vector3(0.0f, 0.0f, 1.0f);
  UnityEngine::Vector3 _bottomPos = UnityEngine::Vector3(0.0f, 0.0f, 0.0f);

  GlobalNamespace::BladeMovementDataElement Modify(GlobalNamespace::BladeMovementDataElement original);
};

DECLARE_CLASS_CODEGEN(Vivify, FollowedSaberTrail, GlobalNamespace::SaberTrail) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Awake);
  DECLARE_INSTANCE_METHOD(void, InitFollowed, GlobalNamespace::SaberTrail* followed, UnityEngine::Transform* parent,
                          UnityEngine::Material* material, UnityEngine::Vector3 topPos,
                          UnityEngine::Vector3 bottomPos, float_t duration, int32_t samplingFrequency,
                          int32_t granularity);
  DECLARE_INSTANCE_METHOD(void, Update);
  DECLARE_INSTANCE_METHOD(void, LateUpdate);
  DECLARE_INSTANCE_METHOD(void, Cleanup);

private:
  GlobalNamespace::SaberTrail* _followed = nullptr;
  OffsetBladeMovementData* _offsetMovementData = nullptr;
  UnityEngine::MaterialPropertyBlock* _materialPropertyBlock = nullptr;
  UnityEngine::Color _lastAppliedColor;
  bool _hasLastAppliedColor = false;
};

DECLARE_CLASS_CODEGEN(Vivify, RuntimeBehaviour, UnityEngine::MonoBehaviour) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Update);
  DECLARE_INSTANCE_METHOD(void, OnDestroy);
};

DECLARE_CLASS_CODEGEN(Vivify, MultipassKeywordController, UnityEngine::MonoBehaviour) {
public:
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, Awake);
  DECLARE_INSTANCE_METHOD(void, OnPreRender);
  DECLARE_INSTANCE_METHOD(void, OnDisable);

private:
  UnityEngine::Camera* _camera = nullptr;
};

#include "VivifyCameraApplier.hpp"
DEFINE_TYPE(Vivify, OffsetBladeMovementData);
DEFINE_TYPE(Vivify, FollowedSaberTrail);
DEFINE_TYPE(Vivify, RuntimeBehaviour);
DEFINE_TYPE(Vivify, MultipassKeywordController);
DEFINE_TYPE(Vivify, CameraApplier);
namespace Vivify {
namespace {
int ColorPropertyId();
constexpr auto kVivifyMultipassKeyword = u"MULTIPASS_ENABLED";
constexpr auto kUnitySinglePassStereoKeyword = u"UNITY_SINGLE_PASS_STEREO";
constexpr auto kUnitySinglePassInstancedKeyword = u"STEREO_INSTANCING_ON";
constexpr auto kUnityStereoInstancingEnabledKeyword = u"UNITY_STEREO_INSTANCING_ENABLED";
constexpr auto kUnitySinglePassMultiviewKeyword = u"STEREO_MULTIVIEW_ON";
constexpr auto kUnityStereoMultiviewEnabledKeyword = u"UNITY_STEREO_MULTIVIEW_ENABLED";
constexpr auto kUnityDoubleWideStereoKeyword = u"STEREO_DOUBLEWIDE_TARGET";
int MultipassEyePropertyId() {
  static int id = UnityEngine::Shader::PropertyToID(u"_StereoActiveEye");
  return id;
}
int UnityStereoEyeIndexPropertyId() {
  static int id = UnityEngine::Shader::PropertyToID(u"unity_StereoEyeIndex");
  return id;
}
int UnityStereoActiveEyePropertyId() {
  static int id = UnityEngine::Shader::PropertyToID(u"_UnityStereoEyeIndex");
  return id;
}
void SetGlobalKeyword(::StringW keyword, bool enabled) {
  if (enabled) {
    UnityEngine::Shader::EnableKeyword(keyword);
  } else {
    UnityEngine::Shader::DisableKeyword(keyword);
  }
}
void SetMultipassShaderState(bool enabled,
                             UnityEngine::XR::XRSettings_StereoRenderingMode stereoMode =
                                 UnityEngine::XR::XRSettings_StereoRenderingMode::MultiPass,
                             int eye = 0) {
  // Cache last applied state — OnPreRender fires up to 2x per frame for stereo.
  // Skipping redundant calls avoids 10 il2cpp boundary crossings per no-op invocation.
  struct MultipassState {
    bool enabled = false;
    int stereoMode = -1;
    int eye = -1;
    bool initialized = false;
  };
  static MultipassState last;
  int const modeVal = stereoMode.value__;
  if (last.initialized && last.enabled == enabled && last.stereoMode == modeVal && last.eye == eye) return;
  last = {enabled, modeVal, eye, true};
  SetGlobalKeyword(kVivifyMultipassKeyword, enabled);
  SetGlobalKeyword(kUnitySinglePassStereoKeyword,
                   enabled && modeVal != UnityEngine::XR::XRSettings_StereoRenderingMode::MultiPass.value__);
  SetGlobalKeyword(kUnitySinglePassInstancedKeyword,
                   enabled && modeVal == UnityEngine::XR::XRSettings_StereoRenderingMode::SinglePassInstanced.value__);
  SetGlobalKeyword(kUnityStereoInstancingEnabledKeyword,
                   enabled && modeVal == UnityEngine::XR::XRSettings_StereoRenderingMode::SinglePassInstanced.value__);
  SetGlobalKeyword(kUnitySinglePassMultiviewKeyword,
                   enabled && modeVal == UnityEngine::XR::XRSettings_StereoRenderingMode::SinglePassMultiview.value__);
  SetGlobalKeyword(kUnityStereoMultiviewEnabledKeyword,
                   enabled && modeVal == UnityEngine::XR::XRSettings_StereoRenderingMode::SinglePassMultiview.value__);
  SetGlobalKeyword(kUnityDoubleWideStereoKeyword,
                   enabled && modeVal == UnityEngine::XR::XRSettings_StereoRenderingMode::SinglePass.value__);
  UnityEngine::Shader::SetGlobalInt(MultipassEyePropertyId(), eye);
  UnityEngine::Shader::SetGlobalInt(UnityStereoEyeIndexPropertyId(), eye);
  UnityEngine::Shader::SetGlobalInt(UnityStereoActiveEyePropertyId(), eye);
}
void SetMultipassShaderStateForCamera(UnityEngine::Camera* camera) {
  if (!GetMultipassRenderingEnabled()) {
    SetMultipassShaderState(false);
    return;
  }
  auto stereoMode = UnityEngine::XR::XRSettings::get_stereoRenderingMode();
  bool const hasCamera = camera != nullptr && UnityEngine::Object::op_Implicit_bool(camera);
  bool const isStereoCamera = hasCamera && camera->get_stereoEnabled() &&
                              camera->get_stereoTargetEye().value__ != UnityEngine::StereoTargetEyeMask::None.value__;
  int const activeEye = hasCamera ? camera->get_stereoActiveEye().value__ : 0;
  SetMultipassShaderState(isStereoCamera, stereoMode, activeEye);
}
bool NearlySameColor(UnityEngine::Color a, UnityEngine::Color b) {
  return std::fabs(a.r - b.r) < 0.001f &&
         std::fabs(a.g - b.g) < 0.001f &&
         std::fabs(a.b - b.b) < 0.001f &&
         std::fabs(a.a - b.a) < 0.001f;
}
bool IsManagedAlive(UnityEngine::Object* object) {
  return object != nullptr && UnityEngine::Object::op_Implicit_bool(object);
}
template <class T>
T* SafeUnityPtr(UnityW<T> object) {
  return IsManagedAlive(object) ? object.unsafePtr() : nullptr;
}
std::string ToStdString(::StringW value) {
  return value ? il2cpp_utils::detail::to_string(value) : std::string("<null>");
}
std::string BoolText(bool value) {
  return value ? "true" : "false";
}
std::string ShaderNameForLog(UnityEngine::Shader* shader) {
  if (!IsManagedAlive(shader)) return "<null>";
  return ToStdString(shader->get_name());
}
bool IsInternalErrorShaderName(std::string_view shaderName) {
  return shaderName.find("Hidden/InternalErrorShader") != std::string_view::npos;
}
std::optional<uint64_t> ReadJsonUint64(rapidjson::Value const& value) {
  if (value.IsUint64()) return value.GetUint64();
  if (value.IsUint()) return value.GetUint();
  if (value.IsString()) {
    try {
      return std::stoull(value.GetString());
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}
std::optional<uint64_t> ReadAndroidBundleChecksum(rapidjson::Value const& object) {
  if (!object.IsObject()) return std::nullopt;
  static constexpr std::string_view bundleKeys[] = {
      "_assetBundle", "assetBundle", "_assetBundleChecksums", "assetBundleChecksums"};
  static constexpr std::string_view androidKeys[] = {"_android2021", "android2021"};
  for (auto bundleKey : bundleKeys) {
    auto bundle = object.FindMember(bundleKey.data());
    if (bundle == object.MemberEnd() || !bundle->value.IsObject()) continue;
    for (auto androidKey : androidKeys) {
      auto checksum = bundle->value.FindMember(androidKey.data());
      if (checksum != bundle->value.MemberEnd()) {
        if (auto parsed = ReadJsonUint64(checksum->value)) return parsed;
      }
    }
  }
  return std::nullopt;
}
UnityEngine::Vector3 AddVectors(UnityEngine::Vector3 left, UnityEngine::Vector3 right) {
  return UnityEngine::Vector3(left.x + right.x, left.y + right.y, left.z + right.z);
}
}

void OffsetBladeMovementData::Init(GlobalNamespace::IBladeMovementData* followed, UnityEngine::Transform* parent,
                                   UnityEngine::Vector3 topPos, UnityEngine::Vector3 bottomPos) {
  _followed = followed;
  _parent = parent;
  _topPos = topPos;
  _bottomPos = bottomPos;
}

float_t OffsetBladeMovementData::get_bladeSpeed() {
  return 0.0f;
}

GlobalNamespace::BladeMovementDataElement OffsetBladeMovementData::get_lastAddedData() {
  if (_followed == nullptr) return {};
  return Modify(_followed->get_lastAddedData());
}

GlobalNamespace::BladeMovementDataElement OffsetBladeMovementData::get_prevAddedData() {
  if (_followed == nullptr) return {};
  return Modify(_followed->get_prevAddedData());
}

GlobalNamespace::BladeMovementDataElement OffsetBladeMovementData::Modify(GlobalNamespace::BladeMovementDataElement original) {
  if (!IsManagedAlive(_parent)) return original;
  return GlobalNamespace::BladeMovementDataElement(
      original.time,
      original.segmentAngle,
      AddVectors(original.bottomPos, _parent->TransformVector(_topPos)),
      AddVectors(original.bottomPos, _parent->TransformVector(_bottomPos)),
      original.segmentNormal);
}

void FollowedSaberTrail::Awake() {}

void FollowedSaberTrail::InitFollowed(GlobalNamespace::SaberTrail* followed, UnityEngine::Transform* parent,
                                      UnityEngine::Material* material, UnityEngine::Vector3 topPos,
                                      UnityEngine::Vector3 bottomPos, float_t duration,
                                      int32_t samplingFrequency, int32_t granularity) {
  if (!IsManagedAlive(followed) || !IsManagedAlive(parent) || !IsManagedAlive(material)) return;
  auto* followedRenderer = SafeUnityPtr(followed->____trailRenderer);
  auto* rendererPrefab = SafeUnityPtr(followed->____trailRendererPrefab);
  if (!IsManagedAlive(followedRenderer) || !IsManagedAlive(rendererPrefab)) return;

  _followed = followed;
  if (_offsetMovementData == nullptr) {
    _offsetMovementData = OffsetBladeMovementData::New_ctor();
  }
  _offsetMovementData->Init(followed->____movementData, parent, topPos, bottomPos);

  ____trailDuration = duration;
  ____samplingFrequency = samplingFrequency;
  ____granularity = granularity;
  ____whiteSectionMaxDuration = followed->____whiteSectionMaxDuration;
  ____movementData = reinterpret_cast<GlobalNamespace::IBladeMovementData*>(_offsetMovementData);
  ____color = followed->____color;

  auto* trailRenderer = SafeUnityPtr(____trailRenderer);
  if (!IsManagedAlive(trailRenderer)) {
    trailRenderer = UnityEngine::Object::Instantiate<GlobalNamespace::SaberTrailRenderer*>(
        rendererPrefab, UnityEngine::Vector3(0.0f, 0.0f, 0.0f), UnityEngine::Quaternion::get_identity());
    ____trailRenderer = trailRenderer;
    if (!IsManagedAlive(trailRenderer)) return;
    auto* followedTransform = SafeUnityPtr(followedRenderer->get_transform());
    UnityEngine::Transform* sourceParentPtr = nullptr;
    if (IsManagedAlive(followedTransform)) {
      auto sourceParent = followedTransform->get_parent();
      sourceParentPtr = SafeUnityPtr(sourceParent);
    }
    auto* trailTransform = SafeUnityPtr(trailRenderer->get_transform());
    if (IsManagedAlive(sourceParentPtr) && IsManagedAlive(trailTransform)) {
      trailTransform->SetParent(sourceParentPtr);
    }
  }

  if (trailRenderer->____meshRenderer) {
    trailRenderer->____meshRenderer->set_material(material);
  }

  Init();
  Update();
  auto* gameObject = SafeUnityPtr(get_gameObject());
  if (IsManagedAlive(gameObject)) {
    gameObject->SetActive(true);
  }
}

void FollowedSaberTrail::Update() {
  if (!IsManagedAlive(_followed)) return;
  auto* trailRenderer = SafeUnityPtr(____trailRenderer);
  if (!IsManagedAlive(trailRenderer) || !trailRenderer->____meshRenderer) return;
  ____color = _followed->____color;
  if (_hasLastAppliedColor && NearlySameColor(____color, _lastAppliedColor)) return;
  if (_materialPropertyBlock == nullptr) {
    _materialPropertyBlock = UnityEngine::MaterialPropertyBlock::New_ctor();
  }
  _materialPropertyBlock->SetColor(ColorPropertyId(), ____color);
  trailRenderer->____meshRenderer->SetPropertyBlock(_materialPropertyBlock);
  _lastAppliedColor = ____color;
  _hasLastAppliedColor = true;
}

void FollowedSaberTrail::LateUpdate() {
  auto* trailRenderer = SafeUnityPtr(____trailRenderer);
  auto* meshRenderer = IsManagedAlive(trailRenderer) ? SafeUnityPtr(trailRenderer->____meshRenderer) : nullptr;
  if (!IsManagedAlive(_followed) || ____movementData == nullptr || !IsManagedAlive(meshRenderer)) {
    Cleanup();
    auto* gameObject = SafeUnityPtr(get_gameObject());
    if (IsManagedAlive(gameObject)) {
      gameObject->SetActive(false);
    }
    return;
  }
  static_cast<GlobalNamespace::SaberTrail*>(this)->LateUpdate();
}

void FollowedSaberTrail::Cleanup() {
  auto* trailRenderer = SafeUnityPtr(____trailRenderer);
  if (IsManagedAlive(trailRenderer)) {
    auto* gameObject = SafeUnityPtr(trailRenderer->get_gameObject());
    if (IsManagedAlive(gameObject)) {
      UnityEngine::Object::Destroy(gameObject);
    }
  }
  ____trailRenderer = nullptr;
  if (_materialPropertyBlock != nullptr) {
    _materialPropertyBlock->Dispose();
    _materialPropertyBlock = nullptr;
  }
  _followed = nullptr;
  _hasLastAppliedColor = false;
}

void MultipassKeywordController::Awake() {
  auto* gameObject = SafeUnityPtr(get_gameObject());
  if (IsManagedAlive(gameObject)) {
    _camera = gameObject->GetComponent<UnityEngine::Camera*>();
  }
}

void MultipassKeywordController::OnPreRender() {
  if (!IsManagedAlive(_camera)) {
    auto* gameObject = SafeUnityPtr(get_gameObject());
    _camera = IsManagedAlive(gameObject) ? gameObject->GetComponent<UnityEngine::Camera*>() : nullptr;
  }
  SetMultipassShaderStateForCamera(_camera);
}

void MultipassKeywordController::OnDisable() {
  SetMultipassShaderState(false);
}

namespace {
constexpr std::string_view kCapability = "Vivify"sv;
constexpr std::string_view kBundleFile = "bundleAndroid2021.vivify"sv;
constexpr bool kAllowUnsafeWindowsBundleFallback = true;
constexpr std::string_view kInstantiatePrefabEvent = "InstantiatePrefab"sv;
constexpr std::string_view kDestroyObjectEvent = "DestroyObject"sv;
constexpr std::string_view kSetMaterialPropertyEvent = "SetMaterialProperty"sv;
constexpr std::string_view kSetAnimatorPropertyEvent = "SetAnimatorProperty"sv;
constexpr std::string_view kSetGlobalPropertyEvent = "SetGlobalProperty"sv;
constexpr std::string_view kAssignObjectPrefabEvent = "AssignObjectPrefab"sv;
constexpr std::string_view kBlitEvent = "Blit"sv;
constexpr std::string_view kPostProcessEvent = "PostProcess"sv;
constexpr std::string_view kPostProcessingEvent = "PostProcessing"sv;
constexpr std::string_view kScreenEffectEvent = "ScreenEffect"sv;
constexpr std::string_view kCreateCameraEvent = "CreateCamera"sv;
constexpr std::string_view kCreateScreenTextureEvent = "CreateScreenTexture"sv;
constexpr std::string_view kSetCameraPropertyEvent = "SetCameraProperty"sv;
constexpr std::string_view kSetRenderingSettingsEvent = "SetRenderingSettings"sv;
constexpr int kMaxActiveBlitEffects = 96;
constexpr int kMaxRenderTextureSize = 4096;
constexpr float kMaxTrailDuration = 2.0f;
constexpr int kMaxTrailSamplingFrequency = 120;
constexpr int kMaxTrailGranularity = 240;
constexpr int kGameplayOverlaySortingOrder = 32767;
enum class MaterialPropertyKind {
  Unsupported,
  Texture,
  Color,
  Float,
  Int,
  Vector,
  Keyword,
};
enum class AnimatorPropertyKind {
  Unsupported,
  Bool,
  Float,
  Integer,
  Trigger,
};
using MaterialValue = std::variant<std::monostate, std::string, bool, float, int, PointDefinitionW>;
using AnimatorValue = std::variant<std::monostate, bool, float, int, PointDefinitionW>;
using SavedGlobalValue = std::variant<UnityEngine::Texture*, UnityEngine::Color, float, UnityEngine::Vector4>;
struct MaterialPropertyChange {
  std::variant<int, std::string> id;
  MaterialPropertyKind kind = MaterialPropertyKind::Unsupported;
  MaterialValue value;
};
struct AnimatorPropertyChange {
  std::string name;
  AnimatorPropertyKind kind = AnimatorPropertyKind::Unsupported;
  AnimatorValue value;
};
struct InstantiatePrefabData {
  std::string asset;
  std::optional<std::string> id;
  Tracks::TransformData transformData;
  std::vector<TrackW> tracks;
  UnityEngine::GameObject* instance = nullptr;
};
struct LivePrefab {
  UnityEngine::GameObject* gameObject = nullptr;
  std::vector<TrackW> tracks;
  std::vector<UnityEngine::Animator*> animators;
};
struct ActiveMaterialAnimation {
  UnityEngine::Material* material = nullptr;
  std::vector<MaterialPropertyChange> properties;
  float startTime = 0.0f;
  float duration = 0.0f;
  float endTime = 0.0f; // startTime + duration, precomputed to avoid per-frame addition
  Functions easing = Functions::EaseLinear;
};
struct ActiveGlobalAnimation {
  std::vector<MaterialPropertyChange> properties;
  float startTime = 0.0f;
  float duration = 0.0f;
  float endTime = 0.0f;
  Functions easing = Functions::EaseLinear;
};
struct ActiveAnimatorAnimation {
  std::string prefabId;
  std::vector<AnimatorPropertyChange> properties;
  float startTime = 0.0f;
  float duration = 0.0f;
  float endTime = 0.0f;
  Functions easing = Functions::EaseLinear;
};
enum class PostProcessingOrder { BeforeMainEffect, AfterMainEffect };
struct BlitMaterialData {
  UnityEngine::Material* material = nullptr;
  int priority = 0;
  std::string source;
  std::vector<std::string> targets;
  int pass = -1;
  std::optional<int> frame;
  std::string asset;
  float eventBeat = 0.0f;
  bool operator<(BlitMaterialData const& o) const { return priority < o.priority; }
};
struct ActiveBlitEffect {
  BlitMaterialData data;
  float expireTime = 0.0f;
};
struct DeclaredTextureData {
  std::string name;
  int propertyId = 0;
  float xRatio = 1.0f;
  float yRatio = 1.0f;
  std::optional<int> width;
  std::optional<int> height;
  std::optional<UnityEngine::RenderTextureFormat> format;
  std::optional<UnityEngine::FilterMode> filterMode;
  UnityEngine::RenderTexture* texture = nullptr;
};
struct CameraPropertyData {
  std::optional<UnityEngine::DepthTextureMode> depthTextureMode;
  std::optional<UnityEngine::CameraClearFlags> clearFlags;
  std::optional<UnityEngine::Color> backgroundColor;
  std::optional<bool> bloomPrePass;
  std::optional<bool> mainEffect;
  std::vector<TrackW> cullingTracks;
  bool cullingWhitelist = true;
  std::optional<int> cullingMask;
};
struct SecondaryCameraData {
  std::string name;
  std::optional<std::string> textureName;
  std::optional<std::string> depthTextureName;
  std::optional<int> texturePropertyId;
  std::optional<int> depthTexturePropertyId;
  CameraPropertyData properties;
  UnityEngine::Camera* camera = nullptr;
  UnityEngine::RenderTexture* colorRT = nullptr;
  UnityEngine::RenderTexture* depthRT = nullptr;
};
enum class RenderSettingKind { Float, Color, Bool, Int, Enum, Material, Light };
struct RenderSettingValue {
  std::string name;
  RenderSettingKind kind = RenderSettingKind::Float;
  std::variant<std::monostate, float, UnityEngine::Color, int, bool, std::string, PointDefinitionW> value;
};
struct SavedRenderSetting {
  std::string name;
  RenderSettingKind kind;
  std::variant<float, UnityEngine::Color, int, bool, UnityEngine::Material*, UnityEngine::Light*> saved;
};
struct ActiveRenderSettingAnimation {
  std::vector<RenderSettingValue> settings;
  float startTime = 0.0f;
  float duration = 0.0f;
  float endTime = 0.0f;
  Functions easing = Functions::EaseLinear;
};
enum class AssignedPrefabKind { Object, AnyDirectionObject, Debris, Trail };
struct AssignedPrefabInfo {
  std::string asset;
  std::vector<TrackW> tracks;
  std::string objectType;
  std::optional<int> saberType;
  AssignedPrefabKind kind = AssignedPrefabKind::Object;
  bool additive = false;
  std::optional<UnityEngine::Vector3> trailTopPos;
  std::optional<UnityEngine::Vector3> trailBottomPos;
  std::optional<float> trailDuration;
  std::optional<int> trailSamplingFrequency;
  std::optional<int> trailGranularity;
};
struct VisualReplacement {
  std::vector<UnityEngine::GameObject*> spawnedObjects;
  std::vector<UnityEngine::Renderer*> disabledRenderers;
  std::vector<UnityEngine::Renderer*> replacementRenderers;
  std::vector<FollowedSaberTrail*> followedTrails;
  GlobalNamespace::MaterialPropertyBlockController* materialPropertyBlockController = nullptr;
  ArrayW<UnityW<UnityEngine::Renderer>, Array<UnityW<UnityEngine::Renderer>>*> originalMaterialBlockRenderers;
  bool hasOriginalMaterialBlockRenderers = false;
  UnityEngine::MaterialPropertyBlock* saberColorPropertyBlock = nullptr;
  UnityEngine::Color lastSaberColor;
  bool hasLastSaberColor = false;
};
struct ActiveSaberVisual {
  GlobalNamespace::SaberModelController* controller = nullptr;
  GlobalNamespace::Saber* saber = nullptr;
  UnityEngine::Transform* parent = nullptr;
};
bool IsVivifyEvent(std::string_view type) {
  // unordered_set gives O(1) average vs the previous O(n) chain of == comparisons.
  static std::unordered_set<std::string_view> const kVivifyEvents = {
      kInstantiatePrefabEvent, kDestroyObjectEvent, kSetMaterialPropertyEvent,
      kSetAnimatorPropertyEvent, kSetGlobalPropertyEvent, kAssignObjectPrefabEvent,
      kBlitEvent, kPostProcessEvent, kPostProcessingEvent, kScreenEffectEvent,
      kCreateCameraEvent, kCreateScreenTextureEvent, kSetCameraPropertyEvent,
      kSetRenderingSettingsEvent,
  };
  return kVivifyEvents.count(type) > 0;
}
bool IsSupportedEvent(std::string_view type) {
  return IsVivifyEvent(type);
}
bool IsPostProcessingEvent(std::string_view type) {
  return type == kBlitEvent || type == kPostProcessEvent ||
         type == kPostProcessingEvent || type == kScreenEffectEvent;
}
std::string NormalizeAssetKey(std::string_view input) {
  std::string key(input);
  for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return key;
}
std::string JoinPath(std::string_view left, std::string_view right) {
  std::string result(left);
  if (!result.empty() && result.back() != '/' && result.back() != '\\') {
    result.push_back('/');
  }
  result.append(right);
  return result;
}
std::optional<std::string_view> ReadStringView(rapidjson::Value const& object, std::string_view key) {
  auto member = object.FindMember(key.data());
  if (member == object.MemberEnd() || !member->value.IsString()) {
    return std::nullopt;
  }
  return member->value.GetString();
}
std::optional<float> ReadFloat(rapidjson::Value const& object, std::string_view key) {
  auto member = object.FindMember(key.data());
  if (member == object.MemberEnd()) {
    return std::nullopt;
  }
  if (member->value.IsNumber()) {
    return member->value.GetFloat();
  }
  if (member->value.IsBool()) {
    return member->value.GetBool() ? 1.0f : 0.0f;
  }
  return std::nullopt;
}
std::optional<int> ReadInt(rapidjson::Value const& object, std::string_view key) {
  auto member = object.FindMember(key.data());
  if (member == object.MemberEnd()) {
    return std::nullopt;
  }
  if (member->value.IsInt()) {
    return member->value.GetInt();
  }
  if (member->value.IsNumber()) {
    return static_cast<int>(member->value.GetFloat());
  }
  return std::nullopt;
}
std::optional<bool> ReadBool(rapidjson::Value const& object, std::string_view key) {
  auto member = object.FindMember(key.data());
  if (member == object.MemberEnd()) {
    return std::nullopt;
  }
  auto const& value = member->value;
  if (value.IsBool()) {
    return value.GetBool();
  }
  if (value.IsNumber()) {
    return value.GetFloat() != 0.0f;
  }
  if (value.IsString()) {
    return std::string_view(value.GetString()) == "true"sv;
  }
  return std::nullopt;
}
std::optional<UnityEngine::Vector3> ReadVector3(rapidjson::Value const& object, std::string_view key) {
  auto member = object.FindMember(key.data());
  if (member == object.MemberEnd() || !member->value.IsArray() || member->value.Size() < 3) {
    return std::nullopt;
  }
  auto arr = member->value.GetArray();
  if (!arr[0].IsNumber() || !arr[1].IsNumber() || !arr[2].IsNumber()) {
    return std::nullopt;
  }
  return UnityEngine::Vector3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
}
rapidjson::Value const* ReadValuePtr(rapidjson::Value const& object, std::string_view key) {
  auto member = object.FindMember(key.data());
  return member == object.MemberEnd() ? nullptr : &member->value;
}
enum class AssetValueState { Missing, Null, String };
struct AssetValue {
  AssetValueState state = AssetValueState::Missing;
  std::string value;
};
AssetValue ReadAssetValue(rapidjson::Value const& object, std::string_view key) {
  auto* value = ReadValuePtr(object, key);
  if (value == nullptr) {
    return {};
  }
  if (value->IsNull()) {
    return {.state = AssetValueState::Null};
  }
  if (value->IsString()) {
    return {.state = AssetValueState::String, .value = value->GetString()};
  }
  return {};
}
std::vector<std::string> ReadStringListOrSingle(rapidjson::Value const& object, std::string_view key) {
  std::vector<std::string> result;
  auto value = ReadValuePtr(object, key);
  if (value == nullptr) {
    return result;
  }
  if (value->IsString()) {
    result.emplace_back(value->GetString());
    return result;
  }
  if (!value->IsArray()) {
    return result;
  }
  result.reserve(value->Size());
  for (auto const& entry : value->GetArray()) {
    if (entry.IsString()) {
      result.emplace_back(entry.GetString());
    }
  }
  return result;
}
Functions ParseEasing(std::string_view easing) {
  static std::unordered_map<std::string_view, Functions> const kEasingMap = {
      {"easeStep"sv, Functions::EaseStep},
      {"easeInQuad"sv, Functions::EaseInQuad},
      {"easeOutQuad"sv, Functions::EaseOutQuad},
      {"easeInOutQuad"sv, Functions::EaseInOutQuad},
      {"easeInCubic"sv, Functions::EaseInCubic},
      {"easeOutCubic"sv, Functions::EaseOutCubic},
      {"easeInOutCubic"sv, Functions::EaseInOutCubic},
      {"easeInQuart"sv, Functions::EaseInQuart},
      {"easeOutQuart"sv, Functions::EaseOutQuart},
      {"easeInOutQuart"sv, Functions::EaseInOutQuart},
      {"easeInQuint"sv, Functions::EaseInQuint},
      {"easeOutQuint"sv, Functions::EaseOutQuint},
      {"easeInOutQuint"sv, Functions::EaseInOutQuint},
      {"easeInSine"sv, Functions::EaseInSine},
      {"easeOutSine"sv, Functions::EaseOutSine},
      {"easeInOutSine"sv, Functions::EaseInOutSine},
      {"easeInCirc"sv, Functions::EaseInCirc},
      {"easeOutCirc"sv, Functions::EaseOutCirc},
      {"easeInOutCirc"sv, Functions::EaseInOutCirc},
      {"easeInExpo"sv, Functions::EaseInExpo},
      {"easeOutExpo"sv, Functions::EaseOutExpo},
      {"easeInOutExpo"sv, Functions::EaseInOutExpo},
      {"easeInElastic"sv, Functions::EaseInElastic},
      {"easeOutElastic"sv, Functions::EaseOutElastic},
      {"easeInOutElastic"sv, Functions::EaseInOutElastic},
      {"easeInBack"sv, Functions::EaseInBack},
      {"easeOutBack"sv, Functions::EaseOutBack},
      {"easeInOutBack"sv, Functions::EaseInOutBack},
      {"easeInBounce"sv, Functions::EaseInBounce},
      {"easeOutBounce"sv, Functions::EaseOutBounce},
      {"easeInOutBounce"sv, Functions::EaseInOutBounce},
  };
  auto it = kEasingMap.find(easing);
  return it != kEasingMap.end() ? it->second : Functions::EaseLinear;
}
MaterialPropertyKind ParseMaterialPropertyKind(std::string_view kind) {
  static std::unordered_map<std::string_view, MaterialPropertyKind> const kMap = {
      {"Texture"sv, MaterialPropertyKind::Texture},
      {"Color"sv, MaterialPropertyKind::Color},
      {"Float"sv, MaterialPropertyKind::Float},
      {"Int"sv, MaterialPropertyKind::Int},
      {"Vector"sv, MaterialPropertyKind::Vector},
      {"Keyword"sv, MaterialPropertyKind::Keyword},
  };
  auto it = kMap.find(kind);
  return it != kMap.end() ? it->second : MaterialPropertyKind::Unsupported;
}
AnimatorPropertyKind ParseAnimatorPropertyKind(std::string_view kind) {
  static std::unordered_map<std::string_view, AnimatorPropertyKind> const kMap = {
      {"Bool"sv, AnimatorPropertyKind::Bool},
      {"Float"sv, AnimatorPropertyKind::Float},
      {"Integer"sv, AnimatorPropertyKind::Integer},
      {"Trigger"sv, AnimatorPropertyKind::Trigger},
  };
  auto it = kMap.find(kind);
  return it != kMap.end() ? it->second : AnimatorPropertyKind::Unsupported;
}
UnityEngine::Color VectorToColor(NEVector::Vector4 value) {
  return UnityEngine::Color(value.x, value.y, value.z, value.w);
}
UnityEngine::Vector4 ToUnityVector(NEVector::Vector4 value) {
  return UnityEngine::Vector4(value.x, value.y, value.z, value.w);
}
int ColorPropertyId() {
  static int id = UnityEngine::Shader::PropertyToID(u"_Color");
  return id;
}
constexpr int kTrackCullingLayerBase = 25;
constexpr int kTrackCullingLayerCount = 4;
constexpr int kVisualOnlyLayer = 4; // Unity IgnoreRaycast
class Runtime {
public:
  static Runtime& Instance() {
    static Runtime runtime;
    return runtime;
  }
  CustomJSONData::CustomBeatmapData* GetCurrentBeatmapData() const { return _currentBeatmapData; }
  bool IsResetting() const { return _isResetting; }
  std::vector<AssignedPrefabInfo*> FindAssignedPrefabs(std::string_view objectType,
                                                       GlobalNamespace::NoteData* noteData,
                                                       AssignedPrefabKind kind) {
    std::vector<AssignedPrefabInfo*> result;
    for (auto& info : _assignedPrefabs) {
      if (info.objectType != objectType || info.kind != kind) continue;
      if (AssignmentMatchesTracks(info, noteData)) {
        result.emplace_back(&info);
      }
    }
    return result;
  }
  std::vector<AssignedPrefabInfo*> FindAssignedSaberPrefabs(int type) {
    std::vector<AssignedPrefabInfo*> result;
    for (auto& info : _assignedPrefabs) {
      if (info.objectType == "saber" && info.kind == AssignedPrefabKind::Object &&
          info.saberType.has_value() && info.saberType.value() == type) {
        result.emplace_back(&info);
      }
    }
    return result;
  }
  std::vector<AssignedPrefabInfo*> FindAssignedSaberTrailPrefabs(int type) {
    std::vector<AssignedPrefabInfo*> result;
    for (auto& info : _assignedPrefabs) {
      if (info.objectType == "saber" && info.kind == AssignedPrefabKind::Trail &&
          info.saberType.has_value() && info.saberType.value() == type) {
        result.emplace_back(&info);
      }
    }
    return result;
  }
  std::vector<AssignedPrefabInfo*> FindAssignedDebrisPrefabs(GlobalNamespace::NoteData* noteData) {
    if (noteData == nullptr) return {};
    auto gameplayType = noteData->get_gameplayType();
    if (gameplayType == GlobalNamespace::NoteData_GameplayType::Normal) {
      return FindAssignedPrefabs("colorNotes", noteData, AssignedPrefabKind::Debris);
    }
    if (gameplayType == GlobalNamespace::NoteData_GameplayType::BurstSliderHead) {
      return FindAssignedPrefabs("burstSliders", noteData, AssignedPrefabKind::Debris);
    }
    if (gameplayType == GlobalNamespace::NoteData_GameplayType::BurstSliderElement) {
      return FindAssignedPrefabs("burstSliderElements", noteData, AssignedPrefabKind::Debris);
    }
    return {};
  }
  void PushActiveDebrisPrefabs(std::vector<AssignedPrefabInfo*> infos) {
    _lastCutDebrisPrefabs = infos;
    _activeDebrisPrefabStack.emplace_back(std::move(infos));
  }
  void PopActiveDebrisPrefabs() {
    if (!_activeDebrisPrefabStack.empty()) {
      _activeDebrisPrefabStack.pop_back();
    }
    _lastCutDebrisPrefabs.clear();
  }
  void RestoreNoteVisuals(GlobalNamespace::NoteController* noteController) {
    auto it = _noteReplacements.find(noteController);
    if (it == _noteReplacements.end()) return;
    RestoreReplacementData(it->second);
    _noteReplacements.erase(it);
  }
  void RestoreDebrisVisuals(GlobalNamespace::NoteDebris* debris) {
    auto it = _debrisReplacements.find(debris);
    if (it == _debrisReplacements.end()) return;
    RestoreReplacementData(it->second);
    _debrisReplacements.erase(it);
  }
  void RestoreSaberVisuals(GlobalNamespace::SaberModelController* smc) {
    auto it = _saberReplacements.find(smc);
    if (it == _saberReplacements.end()) return;
    RestoreReplacementData(it->second);
    _saberReplacements.erase(it);
  }
  void CleanCustomObject(UnityEngine::GameObject* go) {
    if (go == nullptr || !UnityEngine::Object::op_Implicit_bool(go)) return;
    auto rigidbodies = go->GetComponentsInChildren<UnityEngine::Rigidbody*>(true);
    for (int i = 0; i < rigidbodies.size(); i++) {
      rigidbodies[i]->set_isKinematic(true);
      rigidbodies[i]->set_useGravity(false);
    }
    auto colliders = go->GetComponentsInChildren<UnityEngine::Collider*>(true);
    for (int i = 0; i < colliders.size(); i++) {
      colliders[i]->set_enabled(false);
    }
    auto noteControllers = go->GetComponentsInChildren<GlobalNamespace::NoteControllerBase*>(true);
    for (int i = 0; i < noteControllers.size(); i++) {
      auto* controller = noteControllers[i];
      if (IsAlive(controller)) controller->set_enabled(false);
    }
    auto cuttables = go->GetComponentsInChildren<GlobalNamespace::CuttableBySaber*>(true);
    for (int i = 0; i < cuttables.size(); i++) {
      auto* cuttable = cuttables[i];
      if (!IsAlive(cuttable)) continue;
      cuttable->set_canBeCut(false);
      cuttable->set_enabled(false);
    }
    auto boxCuttables = go->GetComponentsInChildren<GlobalNamespace::BoxCuttableBySaber*>(true);
    for (int i = 0; i < boxCuttables.size(); i++) {
      auto* cuttable = boxCuttables[i];
      if (!IsAlive(cuttable)) continue;
      cuttable->set_canBeCut(false);
      cuttable->____canBeCut = false;
      if (cuttable->____collider) {
        auto* collider = cuttable->____collider.unsafePtr();
        if (IsAlive(collider)) collider->set_enabled(false);
      }
      cuttable->set_enabled(false);
    }
    auto sphereCuttables = go->GetComponentsInChildren<GlobalNamespace::SphereCuttableBySaber*>(true);
    for (int i = 0; i < sphereCuttables.size(); i++) {
      auto* cuttable = sphereCuttables[i];
      if (!IsAlive(cuttable)) continue;
      cuttable->set_canBeCut(false);
      cuttable->____canBeCut = false;
      if (cuttable->____collider) {
        auto* collider = cuttable->____collider.unsafePtr();
        if (IsAlive(collider)) collider->set_enabled(false);
      }
      cuttable->set_enabled(false);
    }
    auto videoArray = go->GetComponentsInChildren<UnityEngine::Video::VideoPlayer*>(true);
    for (int i = 0; i < videoArray.size(); i++) {
      if (videoArray[i] != nullptr) {
        _videoPlayers.emplace_back(videoArray[i]);
      }
    }
  }
  void ApplyTrackCameraCullingToNote(GlobalNamespace::NoteController* noteController,
                                     GlobalNamespace::NoteData* noteData) {
    if (!IsAlive(noteController) || noteData == nullptr || _cameraCullingLayers.empty()) return;
    auto* customNoteData = il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(noteData).value_or(nullptr);
    if (customNoteData == nullptr || customNoteData->customData == nullptr) return;

    std::vector<TrackW> tracks;
    try {
      auto& ad = TracksAD::getAD(customNoteData->customData);
      tracks.insert(tracks.end(), ad.tracks.begin(), ad.tracks.end());
    } catch (...) {
    }
    if (customNoteData->customData->value.has_value()) {
      bool const v2 = _currentBeatmapData != nullptr && _currentBeatmapData->v2orEarlier;
      auto parsedTracks = ReadTracks(customNoteData->customData->value.value().get(), v2);
      tracks.insert(tracks.end(), parsedTracks.begin(), parsedTracks.end());
    }

    auto* gameObject = SafeUnityPtr(noteController->get_gameObject());
    ApplyTrackCameraCullingToGameObject(gameObject, tracks);
  }
  void ApplyMaterialPropertyBlockChanges(GlobalNamespace::MaterialPropertyBlockController* mpb,
                                         std::string_view context) {
    if (!IsAlive(mpb)) return;
    try {
      mpb->ApplyChanges();
    } catch (std::exception const& ex) {
      PaperLogger.warn("Vivify MPB ApplyChanges skipped: context={} error={}", context, ex.what());
    } catch (...) {
      PaperLogger.warn("Vivify MPB ApplyChanges skipped: context={} error=unknown", context);
    }
  }
  void ReplaceNoteVisuals(GlobalNamespace::NoteController* noteController, std::vector<AssignedPrefabInfo*> const& infos) {
    if (GetDisableCustomNoteVisuals()) return;
    RestoreNoteVisuals(noteController);
    if (!IsAlive(noteController) || infos.empty()) return;
    UnityEngine::Transform* replacementParent = GetReplacementParent(noteController);
    if (!IsAlive(replacementParent)) return;
    std::vector<AssignedPrefabInfo*> validInfos = GetValidPrefabInfos(infos);
    if (validInfos.empty()) return;

    VisualReplacement replacement;
    // Respect the map's "additive" flag: hide the stock note mesh when the
    // assigned prefab is non-additive (i.e. it wants to be the only visual).
    // The MPB clearing below is still required: Beat Saber uses the note's
    // MaterialPropertyBlockController to apply red/blue saber colors to every
    // renderer in its list.  If we leave the MPB pointing at original renderers
    // while they're disabled the color application is a no-op, which is correct.
    // We must NOT add replacement renderers into the MPB — that would cause BS to
    // overwrite custom prefab materials with saber colors → pitch-black notes.
    bool const hideOriginal = ShouldHideOriginal(validInfos);
    auto originalRenderers = noteController->GetComponentsInChildren<UnityEngine::Renderer*>(true);
    for (auto* info : validInfos) {
      InstantiateReplacementPrefab(*info, replacementParent, replacement, kVisualOnlyLayer);
    }
    if (replacement.spawnedObjects.empty() && replacement.disabledRenderers.empty()) return;

    // Do NOT inject replacement renderers into the MPB. The MPB is used by BS to
    // apply note saber colors (red/blue). If we put the custom prefab's renderers
    // into it, BS overwrites their materials with saber colors → pitch black notes
    // when the material is incompatible. Custom note prefabs own their own colors.
    // We only need to remove original renderers from the MPB when hiding them, so
    // the stock note mesh doesn't show through.
    if (hideOriginal) {
      auto* mpb = GetReplacementMaterialPropertyBlockController(noteController, replacementParent);
      if (IsAlive(mpb)) {
        // Save original renderer list so we can restore it on RestoreNoteVisuals.
        if (!replacement.hasOriginalMaterialBlockRenderers) {
          replacement.materialPropertyBlockController = mpb;
          replacement.originalMaterialBlockRenderers = mpb->____renderers;
          replacement.hasOriginalMaterialBlockRenderers = true;
        }
        // Keep the list empty rather than null; Quest 1.40.8 throws inside
        // ApplyChanges when MaterialPropertyBlockController has no array.
        mpb->____renderers = ArrayW<UnityW<UnityEngine::Renderer>>(static_cast<il2cpp_array_size_t>(0));
        ApplyMaterialPropertyBlockChanges(mpb, "ReplaceNoteVisuals");
      }
      DisableOriginalRenderers(originalRenderers, replacement);
    }
    _noteReplacements[noteController] = std::move(replacement);
  }
  void TrackSaberModel(GlobalNamespace::SaberModelController* smc, GlobalNamespace::Saber* saber, UnityEngine::Transform* parent) {
    if (!IsAlive(smc) || !IsAlive(saber) || !IsAlive(parent)) return;
    PurgeInvalidActiveSabers();
    auto existing = std::find_if(_activeSabers.begin(), _activeSabers.end(), [smc](ActiveSaberVisual const& target) {
      return target.controller == smc;
    });
    if (existing == _activeSabers.end()) {
      _activeSabers.push_back(ActiveSaberVisual{.controller = smc, .saber = saber, .parent = parent});
    } else {
      existing->saber = saber;
      existing->parent = parent;
    }
    auto* smcGameObject = SafeUnityPtr(smc->get_gameObject());
    if (IsAlive(smcGameObject)) {
      ForceGameObjectRenderersOnTop(smcGameObject);
    }
    ApplySaberVisuals(smc, saber, parent);
  }
  void ApplySaberVisualsToActive() {
    PurgeInvalidActiveSabers();
    for (auto const& target : _activeSabers) {
      ApplySaberVisuals(target.controller, target.saber, target.parent);
    }
  }
  std::optional<int> TryGetSaberType(GlobalNamespace::Saber* saber) const {
    if (!IsAlive(saber)) return std::nullopt;
    auto* saberTypeObject = SafeUnityPtr(saber->____saberType);
    if (!IsManagedAlive(saberTypeObject)) return std::nullopt;
    int const value = saberTypeObject->____saberType.value__;
    if (value != GlobalNamespace::SaberType::SaberA.value__ &&
        value != GlobalNamespace::SaberType::SaberB.value__) {
      return std::nullopt;
    }
    return value;
  }
  void ApplySaberVisuals(GlobalNamespace::SaberModelController* smc, GlobalNamespace::Saber* saber, UnityEngine::Transform* parent) {
    RestoreSaberVisuals(smc);
    if (!IsAlive(smc) || !IsAlive(saber) || !IsAlive(parent) || _currentBeatmapData == nullptr || _isResetting) return;
    if (ShouldDisableVisualsForMultiplayer()) return;
    auto saberType = TryGetSaberType(saber);
    if (!saberType.has_value()) return;
    int type = saberType.value();
    auto modelInfos = FindAssignedSaberPrefabs(type);
    auto trailInfos = FindAssignedSaberTrailPrefabs(type);
    auto validModelInfos = GetValidPrefabInfos(modelInfos);
    auto validTrailInfos = GetValidPrefabInfos(trailInfos);
    if (validModelInfos.empty() && validTrailInfos.empty()) return;

    VisualReplacement replacement;
    // Hide the player's existing saber renderers (including any custom saber mod
    // overlays) when the Vivify map specifies a non-additive saber replacement.
    // ShouldHideOriginal returns true when at least one assigned prefab is not
    // marked additive — meaning the map wants its saber to be the only one visible.
    // Quest renderer compatibility:
    // disabling original saber renderers can break CustomSabers
    // and delayed Vivify renderer initialization on Quest.
    bool const shouldHideOriginalSabers = false;
    if (shouldHideOriginalSabers && ShouldHideOriginal(validModelInfos)) {
      DisableOriginalRenderers(smc->get_gameObject(), replacement);
    }
    for (auto* info : validModelInfos) {
      InstantiateReplacementPrefab(*info, parent, replacement);
    }
    ApplySaberTrailVisuals(smc, validTrailInfos, replacement);
    ApplySaberReplacementColor(smc, saber, replacement, true);
    if (!replacement.spawnedObjects.empty() || !replacement.disabledRenderers.empty()) {
      _saberReplacements[smc] = std::move(replacement);
    }
  }
  void ReplaceDebrisVisuals(GlobalNamespace::NoteDebris* debris) {
    if (GetDisableCustomNoteVisuals() || ShouldDisableVisualsForMultiplayer()) return;
    RestoreDebrisVisuals(debris);
    if (!IsAlive(debris)) return;
    auto const& infos = _activeDebrisPrefabStack.empty() ? _lastCutDebrisPrefabs : _activeDebrisPrefabStack.back();
    if (infos.empty()) return;
    auto validInfos = GetValidPrefabInfos(infos);
    if (validInfos.empty()) return;
    UnityEngine::Transform* parent = debris->get_transform();
    if (!IsAlive(parent)) return;

    VisualReplacement replacement;
    bool const hideOriginal = ShouldHideOriginal(validInfos);
    if (hideOriginal) {
      DisableOriginalRenderers(debris->get_gameObject(), replacement);
    }
    for (auto* info : validInfos) {
      InstantiateReplacementPrefab(*info, parent, replacement);
    }
    ApplyReplacementRenderersToMaterialBlock(debris->get_gameObject(), replacement, hideOriginal);
    if (!replacement.spawnedObjects.empty() || !replacement.disabledRenderers.empty()) {
      _debrisReplacements[debris] = std::move(replacement);
    }
  }
  void LateLoad() {
    auto cjdModInfo = CustomJSONData::modInfo.to_c();
    auto tracksModInfo = CModInfo{.id = "Tracks"};
    modloader_require_mod(&cjdModInfo, CMatchType::MatchType_IdOnly);
    modloader_require_mod(&tracksModInfo, CMatchType::MatchType_IdOnly);
    EnsureBehaviour();
    SongCore::API::Capabilities::RegisterCapability(kCapability);
    CustomJSONData::CustomEventCallbacks::AddCustomEventCallback(&Runtime::OnCustomEventStatic);
    SongCore::API::LevelSelect::GetLevelWasSelectedEvent() += [](SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
      Runtime::Instance().HandleLevelSelected(event);
    };
  }
  void Update() {
    // Detect global pause via timescale as a fallback in case the pause menu
    // doesn't notify Vivify. This ensures blit/post effects and animations
    // stop while paused.
    bool const timePaused = UnityEngine::Time::get_timeScale() == 0.0f;
    if (timePaused != _pauseMenuActive) {
      SetPauseMenuActive(timePaused);
    }
    if (_audioTimeSyncController != nullptr && !UnityEngine::Object::op_Implicit_bool(_audioTimeSyncController)) {
      ResetRuntime();
      return;
    }
    if (_currentBeatmapData == nullptr) {
      RefreshCameraComponents(false);
      return;
    }
    UpdateMaterialAnimations();
    UpdateGlobalAnimations();
    UpdateAnimatorAnimations();
    UpdateBlitEffects();
    UpdateRenderSettingAnimations();
    RefreshCameraComponents(true);
    DetectSongRestart();
    SanitizeActiveNoteReplacements();
    UpdateSaberReplacementColors();
  }
  void ApplyBlits(UnityEngine::RenderTexture* src, UnityEngine::RenderTexture* dest) {
    if (!IsAlive(src) || (dest != nullptr && !IsAlive(dest))) return;
    bool const passthroughOnly = GetDisableAllBlits() || _isResetting || _pauseMenuActive ||
                                 _currentBeatmapData == nullptr ||
                                 (_preEffects.empty() && _postEffects.empty());
    if (passthroughOnly) {
      if (GetDisableAllBlits() && GetVivifyDebugLogging())
        PaperLogger.info("Vivify blit passthrough: Disable All Blits is enabled");
      UnityEngine::Graphics::Blit(static_cast<UnityEngine::Texture*>(src), dest);
      return;
    }
    auto* main = EnsureCachedBlitTexture(_mainBlitTexture, src);
    auto* scratch = EnsureCachedBlitTexture(_scratchBlitTexture, src);
    if (!IsAlive(main) || !IsAlive(scratch)) {
      UnityEngine::Graphics::Blit(static_cast<UnityEngine::Texture*>(src), dest);
      return;
    }
    UnityEngine::Graphics::Blit(static_cast<UnityEngine::Texture*>(src), main);
    auto* mainCurrent = main;
    auto* mainScratch = scratch;
    auto renderEffects = [&](std::vector<ActiveBlitEffect> const& effects) {
      for (auto const& effect : effects) {
        auto const& data = effect.data;
        if (!CanUseBlitMaterial(data.material, data.pass)) continue;
        UnityEngine::RenderTexture* blitSrc = nullptr;
        if (data.source == "_Main") {
          blitSrc = mainCurrent;
        } else if (auto it = _declaredTextures.find(data.source); it != _declaredTextures.end()) {
          blitSrc = it->second.texture;
        } else if (auto it = _secondaryCameras.find(data.source); it != _secondaryCameras.end()) {
          blitSrc = it->second.colorRT;
        }
        if (!IsAlive(blitSrc)) continue;
        auto* sTex = static_cast<UnityEngine::Texture*>(blitSrc);
        for (auto const& targetName : data.targets) {
          if (targetName == "_Main") {
            if (data.pass >= 0)
              UnityEngine::Graphics::Blit(sTex, mainScratch, data.material, data.pass);
            else
              UnityEngine::Graphics::Blit(sTex, mainScratch, data.material);
            std::swap(mainCurrent, mainScratch);
          } else if (auto it = _declaredTextures.find(targetName); it != _declaredTextures.end()) {
            if (IsAlive(it->second.texture)) {
              if (data.pass >= 0)
                UnityEngine::Graphics::Blit(sTex, it->second.texture, data.material, data.pass);
              else
                UnityEngine::Graphics::Blit(sTex, it->second.texture, data.material);
            }
          }
        }
      }
    };
    renderEffects(_preEffects);
    renderEffects(_postEffects);
    UnityEngine::Graphics::Blit(static_cast<UnityEngine::Texture*>(mainCurrent), dest);
  }
  UnityEngine::RenderTexture* EnsureCachedBlitTexture(UnityEngine::RenderTexture*& texture,
                                                      UnityEngine::RenderTexture* src) {
    auto desc = src->get_descriptor();
    desc.set_msaaSamples(1);
    desc.set_depthBufferBits(0);
    if (IsAlive(texture)) {
      auto existing = texture->get_descriptor();
      if (existing.get_width() == desc.get_width() &&
          existing.get_height() == desc.get_height() &&
          existing.get_graphicsFormat().value__ == desc.get_graphicsFormat().value__ &&
          texture->IsCreated()) {
        return texture;
      }
      ReleaseRenderTexture(texture);
    }
    texture = UnityEngine::RenderTexture::New_ctor(desc);
    if (!IsAlive(texture)) { texture = nullptr; return nullptr; }
    if (!texture->Create()) { ReleaseRenderTexture(texture); return nullptr; }
    return texture;
  }
  void ReleaseRenderTexture(UnityEngine::RenderTexture*& texture) {
    if (IsAlive(texture)) {
      texture->Release();
      UnityEngine::Object::Destroy(texture);
    }
    texture = nullptr;
  }
  void ReleaseCachedBlitTextures() {
    ReleaseRenderTexture(_mainBlitTexture);
    ReleaseRenderTexture(_scratchBlitTexture);
  }
  void RefreshCameraApplier(UnityEngine::GameObject* mainCamGO, bool allowCameraApplier) {
    if (_pauseMenuActive) allowCameraApplier = false;
    if (!allowCameraApplier) {
      if (_cameraApplier != nullptr && UnityEngine::Object::op_Implicit_bool(_cameraApplier) &&
          _cameraApplier->get_enabled()) {
        _cameraApplier->set_enabled(false);
      }
      return;
    }
    if (!IsAlive(mainCamGO)) { DestroyCameraApplier(); return; }
    if (_cameraApplier == nullptr || !UnityEngine::Object::op_Implicit_bool(_cameraApplier) ||
        _cameraApplier->get_gameObject().unsafePtr() != mainCamGO) {
      DestroyCameraApplier();
      _cameraApplier = mainCamGO->AddComponent<CameraApplier*>();
      _cameraApplier->set_enabled(false);
    }
    bool const needsBlit = !GetDisableAllBlits() && (!_preEffects.empty() || !_postEffects.empty());
    if (_cameraApplier->get_enabled() != needsBlit) _cameraApplier->set_enabled(needsBlit);
  }
  void DestroyCameraApplier() {
    if (_cameraApplier != nullptr && UnityEngine::Object::op_Implicit_bool(_cameraApplier)) {
      _cameraApplier->set_enabled(false);
      UnityEngine::Object::Destroy(_cameraApplier);
    }
    _cameraApplier = nullptr;
  }
  void OnBehaviourDestroyed(RuntimeBehaviour* behaviour) {
    if (_behaviour == behaviour) _behaviour = nullptr;
  }
  void RefreshMultipassRendering() {
    auto mainCam = UnityEngine::Camera::get_main();
    auto mainCamGO = IsAlive(mainCam) ? mainCam->get_gameObject().unsafePtr() : nullptr;
    RefreshMultipassRendering(mainCamGO);
    RefreshLoadedMaterialStereoKeywords();
  }
  void RefreshIsolationSettings() {
    if (GetDisableAllBlits()) {
      if ((!_preEffects.empty() || !_postEffects.empty()) && GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify isolation: clearing active blits pre={} post={}", _preEffects.size(), _postEffects.size());
      }
      _preEffects.clear();
      _postEffects.clear();
      ReleaseCachedBlitTextures();
      DestroyCameraApplier();
    } else if (GetDisableBeat0FilmgrainBlit()) {
      UpdateBlitEffects();
    }

    if (GetDisableCreateCameraDepth()) {
      if (!_secondaryCameras.empty() && GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify isolation: releasing secondary cameras count={}", _secondaryCameras.size());
      }
      for (auto& [name, camera] : _secondaryCameras) {
        ReleaseSecondaryCameraData(camera);
      }
      _secondaryCameras.clear();
      for (auto it = _cameraProperties.begin(); it != _cameraProperties.end();) {
        if (it->first != "_Main") {
          it = _cameraProperties.erase(it);
        } else {
          it++;
        }
      }
    }

    RefreshCameraComponents(_currentBeatmapData != nullptr && !_isResetting && !_pauseMenuActive);
  }
  void SetPauseMenuActive(bool active) {
    if (_currentBeatmapData == nullptr) {
      _pauseMenuActive = false;
      DestroyGameplayOverlayCamera();
      return;
    }
    if (_pauseMenuActive == active) return;
    _pauseMenuActive = active;
    if (active) {
      // ── Disable blit post-processing ────────────────────────────────────
      if (_cameraApplier != nullptr && UnityEngine::Object::op_Implicit_bool(_cameraApplier)) {
        _cameraApplier->set_enabled(false);
      }
      // ── Hide custom/secondary cameras ────────────────────────────────────
      if (_gameplayOverlayCamera != nullptr && UnityEngine::Object::op_Implicit_bool(_gameplayOverlayCamera)) {
        _gameplayOverlayCamera->set_enabled(false);
      }
      for (auto& [name, cam] : _secondaryCameras) {
        if (IsAlive(cam.camera)) cam.camera->set_enabled(false);
      }
      // ── Freeze animations — snapshot and clear so Update() stops ticking ─
      // NOTE: Do NOT call RestoreGlobalProperties/RestoreRenderSettings here.
      // Those functions permanently discard Vivify's overrides and cannot be
      // reversed on resume because the saved-values maps are also cleared.
      // The correct behavior on pause is to simply stop animating; the
      // current animated values remain visible in the pause menu (which is
      // on top of the game world anyway). On resume we restore the animation
      // lists so they continue from where they left off.
      _pausedMaterialAnimations = std::move(_materialAnimations);
      _pausedGlobalAnimations = std::move(_globalAnimations);
      _pausedAnimatorAnimations = std::move(_animatorAnimations);
      _pausedRenderSettingAnimations = std::move(_renderSettingAnimations);
      _materialAnimations.clear();
      _globalAnimations.clear();
      _animatorAnimations.clear();
      _renderSettingAnimations.clear();
      // ── Restore note/saber overlay renderer sorting layers ───────────────
      RestoreOverlayRenderState();
    } else {
      // Resume: restore secondary cameras, re-enable blit, reapply animations.
      for (auto& [name, cam] : _secondaryCameras) {
        if (IsAlive(cam.camera)) cam.camera->set_enabled(true);
      }
      _materialAnimations = std::move(_pausedMaterialAnimations);
      _globalAnimations = std::move(_pausedGlobalAnimations);
      _animatorAnimations = std::move(_pausedAnimatorAnimations);
      _renderSettingAnimations = std::move(_pausedRenderSettingAnimations);
      _pausedMaterialAnimations.clear();
      _pausedGlobalAnimations.clear();
      _pausedAnimatorAnimations.clear();
      _pausedRenderSettingAnimations.clear();
      // Re-register environment prefab culling layers so the overlay camera
      // mask is non-zero and RefreshGameplayOverlayCamera recreates the camera.
      // Guard with audioTimeSyncController alive: SetPauseMenuActive(false) is
      // fired by GamePause_Resume even when the player presses "Menu" to leave
      // (not just "Resume").  If the audio controller is already dead the scene
      // is tearing down — recreating the overlay camera at that point attaches a
      // depth-clear-only camera to the menu scene camera, producing a fully black
      // screen in the main menu.
      bool const audioAlive = _audioTimeSyncController != nullptr &&
                              UnityEngine::Object::op_Implicit_bool(_audioTimeSyncController);
      if (!_isResetting && audioAlive) RebuildGameplayOverlayMaskFromLivePrefabs();
      if (!_isResetting && audioAlive) RefreshCameraComponents(true);
    }
  }
  // Re-register the gameplay overlay layer mask from all still-alive live prefabs.
  // Needed after pause/resume: RestoreOverlayRenderState() clears
  // _gameplayOverlayLayerMask, but environment prefabs are persistent and their
  // layers are only registered once at AssignObject time.  Without this, maps
  // that rely solely on environment prefabs for camera culling (e.g. 42-flux)
  // lose all visuals permanently after the first pause.
  void RebuildGameplayOverlayMaskFromLivePrefabs() {
    for (auto const& [id, prefab] : _livePrefabs) {
      if (!IsAlive(prefab.gameObject)) continue;
      int const layer = FindTrackCullingLayer(prefab.tracks);
      if (layer >= 1 && layer <= 31) {
        _gameplayOverlayLayerMask |= static_cast<int>(1u << static_cast<unsigned>(layer));
      }
    }
  }
  void ForceGameObjectRenderersOnTop(UnityEngine::GameObject* gameObject) {
    if (!IsAlive(gameObject) || _currentBeatmapData == nullptr || _isResetting) return;
    auto renderers = gameObject->GetComponentsInChildren<UnityEngine::Renderer*>(true);
    _overlayRendererSortingOrders.reserve(_overlayRendererSortingOrders.size() + renderers.size());
    for (int i = 0; i < renderers.size(); i++) {
      ForceRendererOnTop(renderers[i]);
    }
  }
private:
  Runtime() = default;
  static void OnCustomEventStatic(GlobalNamespace::BeatmapCallbacksController* callbackController,
                                  CustomJSONData::CustomEventData* customEventData) {
    Runtime::Instance().HandleCustomEvent(callbackController, customEventData);
  }
  TracksAD::BeatmapAssociatedData& GetPointDataSource() {
    return _beatmapAD != nullptr ? *_beatmapAD : _fallbackBeatmapAD;
  }
  void EnsureBehaviour() {
    if (_behaviour != nullptr) {
      return;
    }
    auto* gameObject = UnityEngine::GameObject::New_ctor(u"VivifyRuntime");
    UnityEngine::Object::DontDestroyOnLoad(gameObject);
    _behaviour = gameObject->AddComponent<RuntimeBehaviour*>();
    RefreshCameraComponents(false);
  }
  void RefreshCameraComponents(bool allowCameraApplier) {
    auto mainCam = UnityEngine::Camera::get_main();
    auto mainCamGO = IsAlive(mainCam) ? mainCam->get_gameObject().unsafePtr() : nullptr;
    RefreshMultipassRendering(mainCamGO);
    RefreshGameplayOverlayCamera(mainCam.unsafePtr(), mainCamGO,
                                 _currentBeatmapData != nullptr && !_isResetting && !_pauseMenuActive);
    if (IsAlive(mainCam)) {
      if (auto props = _cameraProperties.find("_Main"); props != _cameraProperties.end()) {
        ApplyCameraProperties(mainCam.unsafePtr(), props->second);
      }
    }
    RefreshCameraApplier(mainCamGO, allowCameraApplier);
  }
  void RefreshMultipassRendering(UnityEngine::GameObject* mainCamGO) {
    if (IsAlive(mainCamGO)) {
      _multipassController = EnsureMultipassKeywordController(mainCamGO);
      return;
    }

    if (_multipassController != nullptr && UnityEngine::Object::op_Implicit_bool(_multipassController)) {
      _multipassController->set_enabled(false);
      UnityEngine::Object::Destroy(_multipassController);
    }
    _multipassController = nullptr;
    SetMultipassShaderState(false);
  }
  MultipassKeywordController* EnsureMultipassKeywordController(UnityEngine::GameObject* gameObject) {
    if (!IsAlive(gameObject)) return nullptr;
    auto* controller = gameObject->GetComponent<MultipassKeywordController*>();
    if (!IsAlive(controller)) {
      controller = gameObject->AddComponent<MultipassKeywordController*>();
    }
    if (IsAlive(controller)) {
      bool const enabled = GetMultipassRenderingEnabled();
      if (controller->get_enabled() != enabled) {
        controller->set_enabled(enabled);
      }
      if (!enabled) {
        SetMultipassShaderState(false);
      }
    }
    return controller;
  }
  int GameplayOverlayLayerMask() const {
    int mask = _gameplayOverlayLayerMask;
    mask |= GlobalNamespace::LayerMasks::getStaticF_noteLayerMask().get_value();
    mask |= GlobalNamespace::LayerMasks::getStaticF_saberLayerMask().get_value();
    mask |= GlobalNamespace::LayerMasks::getStaticF_noteDebrisLayerMask().get_value();
    return mask;
  }
  void RefreshGameplayOverlayCamera(UnityEngine::Camera* mainCam, UnityEngine::GameObject* mainCamGO, bool allowOverlay) {
    int const mask = GameplayOverlayLayerMask();
    if (!allowOverlay || mask == 0 || !IsAlive(mainCam) || !IsAlive(mainCamGO)) {
      DestroyGameplayOverlayCamera();
      return;
    }

    if (_gameplayOverlayCamera == nullptr || !UnityEngine::Object::op_Implicit_bool(_gameplayOverlayCamera)) {
      auto* overlayGO = UnityEngine::GameObject::New_ctor(u"VivifyGameplayOverlayCamera");
      if (!IsAlive(overlayGO)) return;
      _gameplayOverlayCamera = overlayGO->AddComponent<UnityEngine::Camera*>();
      if (!IsAlive(_gameplayOverlayCamera)) {
        UnityEngine::Object::Destroy(overlayGO);
        _gameplayOverlayCamera = nullptr;
        return;
      }
    }

    auto* overlayGO = _gameplayOverlayCamera->get_gameObject().unsafePtr();
    if (!IsAlive(overlayGO)) {
      _gameplayOverlayCamera = nullptr;
      return;
    }
    overlayGO->get_transform()->SetParent(mainCamGO->get_transform(), false);
    _gameplayOverlayCamera->CopyFrom(mainCam);
    _gameplayOverlayCamera->set_clearFlags(UnityEngine::CameraClearFlags::Depth);
    _gameplayOverlayCamera->set_cullingMask(mask);
    _gameplayOverlayCamera->set_depth(mainCam->get_depth() + 1000.0f);
    _gameplayOverlayCamera->set_targetTexture(nullptr);
    _gameplayOverlayCamera->set_stereoTargetEye(mainCam->get_stereoTargetEye());
    EnsureMultipassKeywordController(overlayGO);
    _gameplayOverlayCamera->set_enabled(true);
  }
  void DestroyGameplayOverlayCamera() {
    if (_gameplayOverlayCamera != nullptr && UnityEngine::Object::op_Implicit_bool(_gameplayOverlayCamera)) {
      auto* overlayGO = _gameplayOverlayCamera->get_gameObject().unsafePtr();
      _gameplayOverlayCamera->set_enabled(false);
      if (IsAlive(overlayGO)) {
        UnityEngine::Object::Destroy(overlayGO);
      } else {
        UnityEngine::Object::Destroy(_gameplayOverlayCamera);
      }
    }
    _gameplayOverlayCamera = nullptr;
  }
  std::string ResolveLocalAndroidBundlePath(std::string const& levelPath) const {
    static constexpr std::string_view candidates[] = {
        kBundleFile,
        "bundleandroid2021.vivify"sv,
        "bundleAndroid.vivify"sv,
        "bundleandroid.vivify"sv};
    for (auto candidate : candidates) {
      std::string path = JoinPath(levelPath, candidate);
      if (std::filesystem::exists(path)) return path;
    }
    return JoinPath(levelPath, kBundleFile);
  }
  std::string ResolveUnsupportedWindowsBundlePath(std::string const& levelPath) const {
    static constexpr std::string_view candidates[] = {
        "bundleWindows2021.vivify"sv,
        "bundlewindows2021.vivify"sv,
        "bundleWindows2019.vivify"sv,
        "bundlewindows2019.vivify"sv};
    for (auto candidate : candidates) {
      std::string path = JoinPath(levelPath, candidate);
      if (std::filesystem::exists(path)) return path;
    }
    return {};
  }
  std::string ResolveLocalQuestBundlePath(std::string const& levelPath) const {
    std::string androidPath = ResolveLocalAndroidBundlePath(levelPath);
    if (std::filesystem::exists(androidPath)) return androidPath;
    if constexpr (kAllowUnsafeWindowsBundleFallback) {
      std::string windowsPath = ResolveUnsupportedWindowsBundlePath(levelPath);
      if (!windowsPath.empty()) return windowsPath;
    }
    return androidPath;
  }
  bool IsWindowsBundlePath(std::string const& path) const {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    return lower.find("bundlewindows") != std::string::npos;
  }
  void HandleLevelSelected(SongCore::API::LevelSelect::LevelWasSelectedEventArgs const& event) {
    // Determine the incoming level path before resetting, so we can decide
    // whether to keep the preloaded bundle.
    std::string incomingLevelPath;
    if (event.isCustom && event.customBeatmapLevel != nullptr) {
      incomingLevelPath = std::string(event.customBeatmapLevel->customLevelPath);
    }
    // If the player selected a different map, unload the current bundle now.
    // If it's the same map (e.g. re-selecting after failure), keep the bundle.
    if (!incomingLevelPath.empty() && incomingLevelPath != _selectedLevelPath) {
      if (_mainBundle != nullptr && UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
        _mainBundle->Unload(true);
        _mainBundle = nullptr;
      }
      _preloadedBundlePath.clear();
    }
    ResetRuntime();
    _selectedLevelPath.clear();
    _selectedMapHasVivifyRequirement = false;
    if (!event.isCustom || event.customBeatmapLevel == nullptr) {
      return;
    }
    _selectedLevelPath = std::string(event.customBeatmapLevel->customLevelPath);
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify level selected: path='{}' isCustom={} hasDetails={}",
                       _selectedLevelPath, BoolText(event.isCustom), BoolText(event.customLevelDetails.has_value()));
    }
    if (event.customLevelDetails) {
      auto const& requirements = event.customLevelDetails->difficultyDetails.requirements;
      _selectedMapHasVivifyRequirement = std::any_of(requirements.begin(), requirements.end(), [](std::string const& requirement) {
        return requirement == kCapability;
      });
    }
    if (_selectedMapHasVivifyRequirement) {
      MetaCore::Game::SetScoreSubmission("Vivify", false);
      std::string bundlePath = ResolveLocalQuestBundlePath(_selectedLevelPath);
      bool bundleExists = std::filesystem::exists(bundlePath);
      if (!bundleExists) {
        uint64_t androidChecksum = 0;
        std::string infoPath = JoinPath(_selectedLevelPath, "Info.dat");
        if (!std::filesystem::exists(infoPath)) infoPath = JoinPath(_selectedLevelPath, "info.dat");
        if (std::filesystem::exists(infoPath)) {
          std::ifstream ifs(infoPath);
          if (!ifs.is_open()) return;
          std::string str((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
          rapidjson::Document doc;
          doc.Parse(str.c_str());
          if (!doc.HasParseError()) {
            if (auto checksum = ReadAndroidBundleChecksum(doc)) {
              androidChecksum = *checksum;
            }
            rapidjson::Value const* customData = nullptr;
            if (doc.HasMember("_customData")) customData = &doc["_customData"];
            else if (doc.HasMember("customData")) customData = &doc["customData"];
            if (androidChecksum == 0 && customData && customData->IsObject()) {
              if (auto checksum = ReadAndroidBundleChecksum(*customData)) {
                androidChecksum = *checksum;
              }
            }
          }
        }
        if (GetVivifyDebugLogging()) {
          PaperLogger.info("Vivify bundle selection: localPath='{}' exists={} android2021={}",
                           bundlePath, BoolText(bundleExists), androidChecksum);
        }
        if (androidChecksum != 0) {
          SongCore::API::PlayButton::DisablePlayButton("Vivify", "Downloading assets...");
          DownloadBundle(androidChecksum, _selectedLevelPath, [this](bool success) {
            if (success) {
              SongCore::API::PlayButton::EnablePlayButton("Vivify");
            } else {
              SongCore::API::PlayButton::DisablePlayButton("Vivify", "Failed to download assets.");
            }
          });
        } else {
          std::string windowsBundlePath = ResolveUnsupportedWindowsBundlePath(_selectedLevelPath);
          if (!windowsBundlePath.empty() && kAllowUnsafeWindowsBundleFallback) {
            if (GetVivifyDebugLogging()) {
              PaperLogger.warn("Vivify unsafe fallback: trying Windows bundle on Quest: '{}'", windowsBundlePath);
            }
            SongCore::API::PlayButton::EnablePlayButton("Vivify");
            PreloadBundle(windowsBundlePath);
          } else if (!windowsBundlePath.empty()) {
            if (GetVivifyDebugLogging()) {
              PaperLogger.warn("Vivify found a Windows bundle on Quest and will not load it: '{}'", windowsBundlePath);
            }
            SongCore::API::PlayButton::DisablePlayButton("Vivify", "Windows bundle found; Quest needs an Android bundle.");
          } else {
            SongCore::API::PlayButton::DisablePlayButton("Vivify", "This map does not support your game version.");
          }
        }
      } else {
        if (GetVivifyDebugLogging()) {
          if (IsWindowsBundlePath(bundlePath)) {
            PaperLogger.warn("Vivify unsafe fallback: selected cached Windows bundle on Quest '{}'", bundlePath);
          } else {
            PaperLogger.info("Vivify bundle selection: using cached local bundle '{}'", bundlePath);
          }
        }
        SongCore::API::PlayButton::EnablePlayButton("Vivify");
        // Preload the bundle now (level selection time) so that when the song
        // starts and the first Vivify event fires, LoadMainBundle is a fast
        // no-op rather than a blocking LoadFromFile mid-gameplay. This means
        // all events fire at their correct beat times instead of everything
        // after beat N (the first event's beat) being delayed by bundle load time.
        PreloadBundle(bundlePath);
      }
    } else {
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify level selected: no Vivify requirement, leaving play button and score submission untouched");
      }
    }
  }
  void DownloadBundle(uint64_t checksum, std::string const& levelPath, std::function<void(bool)> callback) {
    std::string url = "https://repo.totalbs.dev/api/v1/bundles/" + std::to_string(checksum);
    std::string bundlePath = JoinPath(levelPath, kBundleFile);
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify bundle download: android2021={} metadataUrl='{}' cachePath='{}'",
                       checksum, url, bundlePath);
    }
    WebUtils::GetAsync<WebUtils::StringResponse>(WebUtils::URLOptions(url), [bundlePath, callback, url](WebUtils::StringResponse res) {
      if (res.IsSuccessful() && res.responseData.has_value()) {
        rapidjson::Document doc;
        doc.Parse(res.responseData->c_str());
        if (!doc.HasParseError() && doc.HasMember("downloadUrl") && doc["downloadUrl"].IsString()) {
          std::string downloadUrl = doc["downloadUrl"].GetString();
          if (GetVivifyDebugLogging()) {
            PaperLogger.info("Vivify bundle download URL resolved: '{}'", downloadUrl);
          }
          WebUtils::GetAsync<WebUtils::DataResponse>(WebUtils::URLOptions(downloadUrl), [bundlePath, callback](WebUtils::DataResponse dataRes) {
            if (dataRes.IsSuccessful() && dataRes.responseData.has_value()) {
              std::ofstream os(bundlePath, std::ios::binary);
              os.write((char*)dataRes.responseData->data(), dataRes.responseData->size());
              os.close();
              if (GetVivifyDebugLogging()) {
                PaperLogger.info("Vivify bundle download complete: path='{}' bytes={}",
                                 bundlePath, dataRes.responseData->size());
              }
              BSML::MainThreadScheduler::Schedule([callback]{
                callback(true);
              });
            } else {
              if (GetVivifyDebugLogging()) {
                PaperLogger.warn("Vivify bundle download failed: data request unsuccessful path='{}'", bundlePath);
              }
              BSML::MainThreadScheduler::Schedule([callback]{
                callback(false);
              });
            }
          });
        } else {
          if (GetVivifyDebugLogging()) {
            PaperLogger.warn("Vivify bundle download failed: metadata response did not contain downloadUrl");
          }
          BSML::MainThreadScheduler::Schedule([callback]{
            callback(false);
          });
        }
      } else {
        if (GetVivifyDebugLogging()) {
          PaperLogger.warn("Vivify bundle download failed: metadata request unsuccessful url='{}'", url);
        }
        BSML::MainThreadScheduler::Schedule([callback]{
          callback(false);
        });
      }
    });
  }
  void HandleCustomEvent(GlobalNamespace::BeatmapCallbacksController* callbackController,
                         CustomJSONData::CustomEventData* customEventData) {
    if (callbackController == nullptr || customEventData == nullptr) return;
    std::string_view type = customEventData->type;
    bool const isVivifyEvent = IsVivifyEvent(type);
    if (!isVivifyEvent && !_selectedMapHasVivifyRequirement && _currentBeatmapData == nullptr) return;
    // EnsureBeatmapPrepared MUST run before the IsVivifyEvent filter.
    // If it ran after, a map whose first custom event is Noodle/Chroma at beat 0
    // and whose first Vivify event is at beat 32 would miss 32 beats of setup.
    // Pass the triggering beat so ApplyMissedVivifyEvents knows exactly how far
    // back to replay — using songTime (seconds) would be wrong because
    // customEventData->time is in beats.
    if (!EnsureBeatmapPrepared(callbackController, customEventData->time)) return;
    if (!isVivifyEvent) return;
    if (_catchUpAppliedCustomEvents.erase(customEventData) > 0) return;
    auto* json = GetEventJson(customEventData);
    if (json == nullptr) return;
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify custom event: type='{}' beat={} songTime={}",
                       std::string(type), customEventData->time, CurrentSongTime());
    }
    if (type == kInstantiatePrefabEvent) {
      InstantiatePrefab(customEventData, *json);
    } else if (type == kDestroyObjectEvent) {
      DestroyObjects(*json);
    } else if (type == kSetMaterialPropertyEvent) {
      HandleSetMaterialProperty(customEventData, *json);
    } else if (type == kSetAnimatorPropertyEvent) {
      HandleSetAnimatorProperty(customEventData, *json);
    } else if (type == kSetGlobalPropertyEvent) {
      HandleSetGlobalProperty(customEventData, *json);
    } else if (IsPostProcessingEvent(type)) {
      HandleBlit(customEventData, *json);
    } else if (type == kCreateCameraEvent) {
      HandleCreateCamera(*json);
    } else if (type == kCreateScreenTextureEvent) {
      HandleCreateScreenTexture(*json);
    } else if (type == kSetCameraPropertyEvent) {
      HandleSetCameraProperty(*json);
    } else if (type == kSetRenderingSettingsEvent) {
      HandleSetRenderingSettings(customEventData, *json);
    } else if (type == kAssignObjectPrefabEvent) {
      HandleAssignObjectPrefab(customEventData, *json);
    }
  }
  CustomJSONData::CustomBeatmapData* GetCustomBeatmapData(GlobalNamespace::BeatmapCallbacksController* callbackController) {
    if (callbackController == nullptr) return nullptr;
    auto* beatmapData = callbackController->_beatmapData;
    if (beatmapData == nullptr) return nullptr;
    return il2cpp_utils::try_cast<CustomJSONData::CustomBeatmapData>(beatmapData).value_or(nullptr);
  }
  bool EnsureBeatmapPrepared(GlobalNamespace::BeatmapCallbacksController* callbackController, float triggerBeat = 0.0f) {
    auto* customBeatmapData = GetCustomBeatmapData(callbackController);
    if (customBeatmapData == nullptr) return false;
    if (_currentBeatmapData == customBeatmapData) return true;
    PrepareBeatmap(customBeatmapData, triggerBeat);
    return _currentBeatmapData == customBeatmapData;
  }
  void PrepareBeatmap(CustomJSONData::CustomBeatmapData* beatmapData, float triggerBeat = 0.0f) {
    ResetRuntime();
    _currentBeatmapData = beatmapData;
    _beatmapAD = nullptr;
    _audioTimeSyncController = UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
    if (beatmapData->customData != nullptr) {
      try {
        TracksAD::readBeatmapDataAD(beatmapData);
        _beatmapAD = &TracksAD::getBeatmapAD(beatmapData->customData);
      } catch (std::exception const& ex) {
      }
    }
    LoadMainBundle();
    PreloadInstantiatePrefabs();
    ApplyMissedVivifyEvents(triggerBeat);
    _lastSongTime = CurrentSongTime();
  }
  // Replay all Vivify events at beat <= triggerBeat (the beat of the first live
  // event that triggered PrepareBeatmap). Using the triggering event's own beat
  // is correct because BeatmapCallbacksController fires events in order — any
  // event at beat <= triggerBeat has already been dispatched (and rejected,
  // since _currentBeatmapData was null at that point). Using songTime (seconds)
  // was wrong: customEventData->time is in beats, not seconds.
  void ApplyMissedVivifyEvents(float upToBeat) {
    if (_currentBeatmapData == nullptr) return;
    _catchUpAppliedCustomEvents.clear();
    for (auto* customEventData : _currentBeatmapData->customEventDatas) {
      if (customEventData == nullptr) continue;
      if (customEventData->time > upToBeat) continue;
      std::string_view type = customEventData->type;
      if (!IsVivifyEvent(type)) continue;
      auto* json = GetEventJson(customEventData);
      if (json == nullptr) continue;
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify catch-up event: type='{}' beat={}", std::string(type), customEventData->time);
      }
      if (type == kInstantiatePrefabEvent) {
        InstantiatePrefab(customEventData, *json);
      } else if (type == kDestroyObjectEvent) {
        DestroyObjects(*json);
      } else if (type == kSetMaterialPropertyEvent) {
        HandleSetMaterialProperty(customEventData, *json);
      } else if (type == kSetAnimatorPropertyEvent) {
        HandleSetAnimatorProperty(customEventData, *json);
      } else if (type == kSetGlobalPropertyEvent) {
        HandleSetGlobalProperty(customEventData, *json);
      } else if (IsPostProcessingEvent(type)) {
        HandleBlit(customEventData, *json);
      } else if (type == kCreateCameraEvent) {
        HandleCreateCamera(*json);
      } else if (type == kCreateScreenTextureEvent) {
        HandleCreateScreenTexture(*json);
      } else if (type == kSetCameraPropertyEvent) {
        HandleSetCameraProperty(*json);
      } else if (type == kSetRenderingSettingsEvent) {
        HandleSetRenderingSettings(customEventData, *json);
      } else if (type == kAssignObjectPrefabEvent) {
        HandleAssignObjectPrefab(customEventData, *json);
      } else {
        continue;
      }
      // Mark as applied so HandleCustomEvent skips it when BeatmapCallbacksController
      // re-fires it at its natural beat time. Without this, every caught-up event
      // would double-fire: once here, once from BCC.
      _catchUpAppliedCustomEvents.emplace(customEventData);
    }
  }
  void ResetRuntime() {
    _isResetting = true;
    DestroyCameraApplier();
    DestroyGameplayOverlayCamera();
    RestoreGlobalProperties();
    RestoreAllVisualReplacements();
    RestoreOverlayRenderState();
    std::unordered_set<UnityEngine::GameObject*> destroyed;
    for (auto& [id, prefab] : _livePrefabs) {
      if (prefab.gameObject == nullptr) {
        continue;
      }
      for (auto const& track : prefab.tracks) {
        track.UnregisterGameObject(prefab.gameObject);
      }
      if (destroyed.emplace(prefab.gameObject).second) {
        if (UnityEngine::Object::op_Implicit_bool(prefab.gameObject)) {
          UnityEngine::Object::Destroy(prefab.gameObject);
        }
      }
    }
    for (auto& [eventData, instantiate] : _instantiatePrefabs) {
      if (instantiate.instance != nullptr && destroyed.emplace(instantiate.instance).second) {
        if (UnityEngine::Object::op_Implicit_bool(instantiate.instance)) {
          UnityEngine::Object::Destroy(instantiate.instance);
        }
      }
      instantiate.instance = nullptr;
    }
    _livePrefabs.clear();
    _instantiatePrefabs.clear();
    _materialAnimations.clear();
    _globalAnimations.clear();
    _animatorAnimations.clear();
    _pausedMaterialAnimations.clear();
    _pausedGlobalAnimations.clear();
    _pausedAnimatorAnimations.clear();
    _pausedRenderSettingAnimations.clear();
    _savedGlobalProperties.clear();
    _savedGlobalKeywords.clear();
    _repairedMaterials.clear();
    _assets.clear();
    _assetsByName.clear();
    _supportedShadersByName.clear();
    _catchUpAppliedCustomEvents.clear();
    for (auto& [name, dt] : _declaredTextures) ReleaseDeclaredTextureData(dt);
    _declaredTextures.clear();
    for (auto& [name, cam] : _secondaryCameras) ReleaseSecondaryCameraData(cam);
    _secondaryCameras.clear();
    _preEffects.clear();
    _postEffects.clear();
    ReleaseCachedBlitTextures();
    RestoreRenderSettings();
    _renderSettingAnimations.clear();
    _savedRenderSettings.clear();
    _assignedPrefabs.clear();
    _activeDebrisPrefabStack.clear();
    _lastCutDebrisPrefabs.clear();
    _videoPlayers.clear();
    _assetPaths.clear();
    _cameraProperties.clear();
    _cameraCullingLayers.clear();
    // Keep _mainBundle loaded so the next LoadMainBundle call for the same map
    // (e.g. Practice mode restart) is instant. LoadMainBundle checks
    // _preloadedBundlePath and skips the blocking AssetBundle::LoadFromFile when
    // the bundle is already in memory. Asset caches (_assets, _assetsByName etc.)
    // are already cleared above and will be rebuilt on the next LoadMainBundle.
    // Only unload if the bundle is somehow null/dead.
    if (_mainBundle != nullptr && !UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
      _mainBundle = nullptr;
      _preloadedBundlePath.clear();
    }
    _currentBeatmapData = nullptr;
    _beatmapAD = nullptr;
    _audioTimeSyncController = nullptr;
    _lastSongTime = -1.0f;
    _songTimeCacheFrame = -1;
    _songTimeCache = 0.0f;
    _pauseMenuActive = false;
    _unsupportedEventWarnings.clear();
    _isResetting = false;
  }
  // Preload the bundle at level selection time so gameplay starts immediately.
  // Only loads if the path has changed since the last preload.
  void PreloadBundle(std::string const& bundlePath) {
    if (_preloadedBundlePath == bundlePath && _mainBundle != nullptr &&
        UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify bundle already preloaded: '{}'", bundlePath);
      }
      return;
    }
    // Unload previous bundle if it was for a different map.
    if (_mainBundle != nullptr && UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
      _mainBundle->Unload(true);
      _mainBundle = nullptr;
    }
    _preloadedBundlePath = bundlePath;
    _mainBundle = UnityEngine::AssetBundle::LoadFromFile(StringW(bundlePath));
    if (_mainBundle == nullptr) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify bundle preload failed: '{}'", bundlePath);
      }
      _preloadedBundlePath.clear();
      return;
    }
    // Prime the asset caches so GetAssetAs lookups are O(1) during gameplay.
    auto assetNames = _mainBundle->GetAllAssetNames();
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify bundle preloaded: '{}' assets={}", bundlePath, assetNames.size());
    }
    _assets.clear();
    _assetsByName.clear();
    _supportedShadersByName.clear();
    for (auto assetName : assetNames) {
      if (!assetName) continue;
      std::string originalAssetPath = il2cpp_utils::detail::to_string(assetName);
      auto asset = _mainBundle->LoadAsset(assetName);
      if (asset == nullptr) continue;
      std::string key = NormalizeAssetKey(originalAssetPath);
      if (!key.empty()) _assets[key] = asset;
      auto name = asset->get_name();
      if (name) {
        auto nameKey = NormalizeAssetKey(il2cpp_utils::detail::to_string(name));
        if (!nameKey.empty() && !_assetsByName.contains(nameKey)) {
          _assetsByName[nameKey] = asset;
        }
        auto* shader = il2cpp_utils::try_cast<UnityEngine::Shader>(asset.unsafePtr()).value_or(nullptr);
        if (IsAlive(shader) && shader->get_isSupported() && !nameKey.empty()) {
          _supportedShadersByName[nameKey] = shader;
        }
      }
      if (!key.empty() && !_assetPaths.contains(key)) _assetPaths[key] = originalAssetPath;
    }
  }
  void LoadMainBundle() {
    LogUnityPlatformInfoOnce();
    if (_selectedLevelPath.empty()) {
      if (_selectedMapHasVivifyRequirement) {
        if (GetVivifyDebugLogging()) {
          PaperLogger.warn("Vivify bundle load skipped: selected level path is empty");
        }
      }
      return;
    }
    std::string bundlePath = ResolveLocalQuestBundlePath(_selectedLevelPath);
    // If the bundle was already preloaded at level selection for this path, rebuild
    // the asset caches from it rather than doing a second LoadFromFile. ResetRuntime
    // clears _assets/_assetsByName/_supportedShadersByName, so they must be refilled
    // here even though the bundle object is already in memory.
    if (!_preloadedBundlePath.empty() && _preloadedBundlePath == bundlePath &&
        _mainBundle != nullptr && UnityEngine::Object::op_Implicit_bool(_mainBundle)) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify bundle preloaded, rebuilding asset caches: '{}'", bundlePath);
      }
      auto assetNames = _mainBundle->GetAllAssetNames();
      for (auto assetName : assetNames) {
        if (!assetName) continue;
        std::string originalAssetPath = il2cpp_utils::detail::to_string(assetName);
        std::string key = NormalizeAssetKey(originalAssetPath);
        _assetPaths[key] = originalAssetPath;
        auto asset = _mainBundle->LoadAsset(assetName);
        if (asset == nullptr) continue;
        _assets[key] = asset;
        auto name = asset->get_name();
        if (name) {
          auto nameKey = NormalizeAssetKey(il2cpp_utils::detail::to_string(name));
          if (!nameKey.empty() && !_assetsByName.contains(nameKey)) {
            _assetsByName[nameKey] = asset;
          }
          if (auto* shader = il2cpp_utils::try_cast<UnityEngine::Shader>(asset.unsafePtr()).value_or(nullptr);
              IsAlive(shader) && shader->get_isSupported() && !nameKey.empty()) {
            _supportedShadersByName[nameKey] = shader;
          }
        }
      }
      return;
    }
    if (GetVivifyDebugLogging()) {
      if (IsWindowsBundlePath(bundlePath)) {
        PaperLogger.warn("Vivify unsafe fallback: LoadFromFile will attempt Windows bundle on Quest path='{}' exists={}",
                         bundlePath, BoolText(std::filesystem::exists(bundlePath)));
      } else {
        PaperLogger.info("Vivify loading asset bundle: path='{}' exists={}",
                         bundlePath, BoolText(std::filesystem::exists(bundlePath)));
      }
    }
    _mainBundle = UnityEngine::AssetBundle::LoadFromFile(StringW(bundlePath));
    if (_mainBundle == nullptr) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify asset bundle load failed: path='{}'", bundlePath);
      }
      if (_selectedMapHasVivifyRequirement) {
      }
      return;
    }
    auto assetNames = _mainBundle->GetAllAssetNames();
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify asset bundle loaded: path='{}' assetCount={}", bundlePath, assetNames.size());
    }
    for (auto assetName : assetNames) {
      if (!assetName) {
        continue;
      }
      std::string originalAssetPath = il2cpp_utils::detail::to_string(assetName);
      std::string key = NormalizeAssetKey(originalAssetPath);
      _assetPaths[key] = originalAssetPath;
      auto asset = _mainBundle->LoadAsset(assetName);
      if (asset != nullptr) {
        _assets[key] = asset;
        auto name = asset->get_name();
        if (name) {
          auto nameKey = NormalizeAssetKey(il2cpp_utils::detail::to_string(name));
          if (!nameKey.empty() && !_assetsByName.contains(nameKey)) {
            _assetsByName[nameKey] = asset;
          }
          if (auto* shader = il2cpp_utils::try_cast<UnityEngine::Shader>(asset.unsafePtr()).value_or(nullptr);
              IsAlive(shader) && shader->get_isSupported() && !nameKey.empty()) {
            _supportedShadersByName[nameKey] = shader;
          }
        }
        if (auto* material = il2cpp_utils::try_cast<UnityEngine::Material>(asset.unsafePtr()).value_or(nullptr);
            IsAlive(material)) {
          LogMaterialShader("bundle-load", originalAssetPath, material);
        } else if (auto* shader = il2cpp_utils::try_cast<UnityEngine::Shader>(asset.unsafePtr()).value_or(nullptr);
                   IsAlive(shader) && GetVivifyDebugLogging()) {
          PaperLogger.info("Vivify shader asset: path='{}' shader='{}' supported={}",
                           originalAssetPath, ShaderNameForLog(shader),
                           BoolText(IsAlive(shader) && shader->get_isSupported()));
        }
      } else if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify asset load failed: path='{}'", originalAssetPath);
      }
    }
    RepairLoadedMaterialShaders();
  }
  UnityEngine::Object* GetAssetObject(std::string_view assetName) const {
    auto it = _assets.find(NormalizeAssetKey(assetName));
    if (it != _assets.end()) {
      return it->second;
    }
    auto nameIt = _assetsByName.find(NormalizeAssetKey(assetName));
    if (nameIt != _assetsByName.end()) {
      return nameIt->second;
    }
    if (GetVivifyDebugLogging()) {
      PaperLogger.warn("Vivify asset lookup miss: '{}'", std::string(assetName));
    }
    return nullptr;
  }
  template <typename T>
  T* GetAssetAs(std::string_view assetName) const {
    auto* asset = GetAssetObject(assetName);
    if (asset == nullptr) {
      return nullptr;
    }
    return il2cpp_utils::try_cast<T>(asset).value_or(nullptr);
  }
  rapidjson::Value const* GetEventJson(CustomJSONData::CustomEventData* customEventData) const {
    if (customEventData->customData == nullptr || !customEventData->customData->value.has_value()) {
      return nullptr;
    }
    return &customEventData->customData->value.value().get();
  }
  std::optional<PointDefinitionW> MakePointDefinition(rapidjson::Value const& object,
                                                      std::string_view key,
                                                      Tracks::ffi::WrapBaseValueType type) {
    auto* value = ReadValuePtr(object, key);
    if (value == nullptr) {
      return std::nullopt;
    }
    if (!value->IsArray() && !value->IsString()) {
      return std::nullopt;
    }
    if (_beatmapAD != nullptr) {
      return _beatmapAD->getPointDefinition(object, key, type);
    }
    if (!value->IsArray()) {
      return std::nullopt;
    }
    return PointDefinitionW(*value, type, _fallbackBeatmapAD.GetBaseProviderContext());
  }
  std::vector<TrackW> ReadTracks(rapidjson::Value const& object, bool v2) {
    if (_beatmapAD == nullptr) {
      return {};
    }
    auto trackKey = v2 ? TracksAD::Constants::V2_TRACK : TracksAD::Constants::TRACK;
    auto tracks = NEJSON::ReadOptionalTracks(object, trackKey, *_beatmapAD);
    if (!tracks.has_value()) {
      return {};
    }
    return {tracks->begin(), tracks->end()};
  }
  bool IsAlive(UnityEngine::Object* object) const {
    return object != nullptr && UnityEngine::Object::op_Implicit_bool(object);
  }

  // Return true if a well-known multiplayer mod is loaded (MultiplayerCore)
  bool IsMultiplayerModLoaded() const {
    CModInfo info{"MultiplayerCore", nullptr, 0};
    auto res = modloader_get_mod(&info, MatchType_IdOnly);
    return res.handle != nullptr;
  }

  bool ShouldDisableVisualsForMultiplayer() const {
    return GetDisableVisualsInMultiplayer() && IsMultiplayerModLoaded();
  }
  int EnsureTrackCullingLayer(TrackW const& track) {
    if (!track) return -1;
    for (auto const& [knownTrack, layer] : _cameraCullingLayers) {
      if (knownTrack == track) return layer;
    }
    if (_cameraCullingLayers.size() >= static_cast<size_t>(kTrackCullingLayerCount)) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify CreateCamera culling skipped: too many unique tracks requested");
      }
      return -1;
    }
    int const layer = kTrackCullingLayerBase + static_cast<int>(_cameraCullingLayers.size());
    _cameraCullingLayers.emplace_back(track, layer);
    return layer;
  }
  std::optional<int> BuildTrackCullingMask(std::vector<TrackW> const& tracks, bool whitelist) {
    if (tracks.empty()) return std::nullopt;
    int trackMask = 0;
    for (auto const& track : tracks) {
      int const layer = EnsureTrackCullingLayer(track);
      if (layer < 0 || layer > 31) continue;
      trackMask |= static_cast<int>(1u << static_cast<unsigned>(layer));
    }
    if (trackMask == 0) return std::nullopt;
    if (whitelist) return trackMask;

    auto* mainCam = SafeUnityPtr(UnityEngine::Camera::get_main());
    int const baseMask = IsAlive(mainCam) ? mainCam->get_cullingMask() : -1;
    return baseMask & ~trackMask;
  }
  int FindTrackCullingLayer(std::vector<TrackW> const& tracks) const {
    if (tracks.empty() || _cameraCullingLayers.empty()) return -1;
    for (auto const& track : tracks) {
      for (auto const& [knownTrack, layer] : _cameraCullingLayers) {
        if (knownTrack == track) return layer;
      }
    }
    return -1;
  }
  void ApplyTrackCameraCullingToGameObject(UnityEngine::GameObject* gameObject,
                                           std::vector<TrackW> const& tracks) {
    int const layer = FindTrackCullingLayer(tracks);
    if (!IsAlive(gameObject) || layer < 0) return;
    // Before changing the layer, collect all cuttable-by-saber GameObjects and
    // their original layers so we can restore them after the recursive layer
    // assignment.  Beat Saber's saber sweep test uses the noteLayerMask, so
    // moving BoxCuttableBySaber / SphereCuttableBySaber GameObjects to a
    // camera-culling-only layer causes the saber to miss them entirely,
    // producing the "ghost block" / saber-passes-through bug.
    struct CuttableLayerRecord {
      UnityEngine::GameObject* go = nullptr;
      int originalLayer = 0;
    };
    std::vector<CuttableLayerRecord> cuttableRecords;
    {
      auto boxCuttables = gameObject->GetComponentsInChildren<GlobalNamespace::BoxCuttableBySaber*>(true);
      cuttableRecords.reserve(cuttableRecords.size() + boxCuttables.size());
      for (int i = 0; i < boxCuttables.size(); i++) {
        auto* c = boxCuttables[i];
        if (!IsAlive(c)) continue;
        auto go = c->get_gameObject();
        auto* goPtr = go ? go.unsafePtr() : nullptr;
        if (IsAlive(goPtr)) {
          cuttableRecords.push_back({goPtr, goPtr->get_layer()});
        }
      }
    }
    {
      auto sphereCuttables = gameObject->GetComponentsInChildren<GlobalNamespace::SphereCuttableBySaber*>(true);
      for (int i = 0; i < sphereCuttables.size(); i++) {
        auto* c = sphereCuttables[i];
        if (!IsAlive(c)) continue;
        auto go = c->get_gameObject();
        auto* goPtr = go ? go.unsafePtr() : nullptr;
        if (IsAlive(goPtr)) {
          cuttableRecords.push_back({goPtr, goPtr->get_layer()});
        }
      }
    }
    SetLayerRecursively(gameObject, layer);
    // Restore the original layers on the cuttable GameObjects so the saber can
    // still detect and cut the note via its standard noteLayerMask.
    for (auto const& rec : cuttableRecords) {
      if (IsAlive(rec.go)) {
        rec.go->set_layer(rec.originalLayer);
      }
    }
    RegisterGameplayOverlayLayer(gameObject);
  }
  void LogUnityPlatformInfoOnce() {
    if (!GetVivifyDebugLogging() || _loggedUnityPlatformInfo) return;
    _loggedUnityPlatformInfo = true;
    auto stereoMode = UnityEngine::XR::XRSettings::get_stereoRenderingMode();
    auto graphicsType = UnityEngine::SystemInfo::get_graphicsDeviceType();
    PaperLogger.info(
        "Vivify platform: os='{}' device='{}' gpu='{}' vendor='{}' api={} stereoMode={} xrOcclusionMesh={} supportsInstancing={} supportsR8={} supportsDepthRT={}",
        ToStdString(UnityEngine::SystemInfo::get_operatingSystem()),
        ToStdString(UnityEngine::SystemInfo::get_deviceModel()),
        ToStdString(UnityEngine::SystemInfo::get_graphicsDeviceName()),
        ToStdString(UnityEngine::SystemInfo::get_graphicsDeviceVendor()),
        graphicsType.value__,
        stereoMode.value__,
        BoolText(UnityEngine::XR::XRSettings::get_useOcclusionMesh()),
        BoolText(UnityEngine::SystemInfo::get_supportsInstancing()),
        BoolText(UnityEngine::SystemInfo::SupportsRenderTextureFormat(UnityEngine::RenderTextureFormat::R8)),
        BoolText(UnityEngine::SystemInfo::SupportsRenderTextureFormat(UnityEngine::RenderTextureFormat::Depth)));
  }
  UnityEngine::RenderTextureFormat SupportedRenderTextureFormat(UnityEngine::RenderTextureFormat requested,
                                                                std::string_view context) const {
    if (UnityEngine::SystemInfo::SupportsRenderTextureFormat(requested)) {
      return requested;
    }
    auto fallback = UnityEngine::RenderTextureFormat::ARGB32;
    if (GetVivifyDebugLogging()) {
      PaperLogger.warn("Vivify RT format unsupported: context={} requested={} fallback={}",
                       context, requested.value__, fallback.value__);
    }
    return fallback;
  }
  void LogMaterialShader(std::string_view context, std::string_view assetPath, UnityEngine::Material* material) const {
    if (!GetVivifyDebugLogging()) return;
    if (!IsAlive(material)) {
      PaperLogger.warn("Vivify material missing: context={} asset={}", context, assetPath);
      return;
    }
    auto* shader = material->get_shader().unsafePtr();
    auto shaderName = ShaderNameForLog(shader);
    PaperLogger.info("Vivify material: context={} asset={} material='{}' shader='{}' supported={} internalError={}",
                     context,
                     assetPath,
                     ToStdString(material->get_name()),
                     shaderName,
                     BoolText(IsAlive(shader) && shader->get_isSupported()),
                     BoolText(IsInternalErrorShaderName(shaderName)));
  }
  void RegisterGameplayOverlayLayer(UnityEngine::GameObject* gameObject) {
    if (!IsAlive(gameObject)) return;
    int const layer = gameObject->get_layer();
    if (layer <= 0 || layer > 31) return;
    _gameplayOverlayLayerMask |= static_cast<int>(1u << static_cast<unsigned>(layer));
  }
  void SetLayerRecursively(UnityEngine::GameObject* gameObject, int layer) {
    if (!IsAlive(gameObject) || layer < 0 || layer > 31) return;
    gameObject->set_layer(layer);
    auto transform = gameObject->get_transform();
    if (!IsAlive(transform)) return;
    int const childCount = transform->get_childCount();
    for (int i = 0; i < childCount; i++) {
      auto* child = transform->GetChild(i).unsafePtr();
      if (!IsAlive(child)) continue;
      SetLayerRecursively(child->get_gameObject().unsafePtr(), layer);
    }
  }
  void ForceRendererOnTop(UnityEngine::Renderer* renderer) {
    if (!IsAlive(renderer)) return;
    if (!_overlayRendererSortingOrders.contains(renderer)) {
      _overlayRendererSortingOrders.emplace(renderer, renderer->get_sortingOrder());
    }
    renderer->set_sortingOrder(kGameplayOverlaySortingOrder);
    RegisterGameplayOverlayLayer(renderer->get_gameObject().unsafePtr());
  }
  void RestoreOverlayRenderState() {
    for (auto const& [renderer, sortingOrder] : _overlayRendererSortingOrders) {
      if (IsAlive(renderer)) {
        renderer->set_sortingOrder(sortingOrder);
      }
    }
    _overlayRendererSortingOrders.clear();
    _gameplayOverlayLayerMask = 0;
  }
  bool CanUseBlitMaterial(UnityEngine::Material* material, int pass) const {
    if (!IsAlive(material)) {
      if (GetVivifyDebugLogging()) PaperLogger.warn("Vivify CB material invalid: null");
      return false;
    }
    auto shader = material->get_shader();
    auto* rawShader = shader.unsafePtr();
    auto shaderName = ShaderNameForLog(rawShader);
    if (!IsAlive(rawShader) || !rawShader->get_isSupported() || IsInternalErrorShaderName(shaderName)) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify CB material invalid: material='{}' shader='{}' supported={} internalError={} pass={}",
                         ToStdString(material->get_name()), shaderName,
                         BoolText(IsAlive(rawShader) && rawShader->get_isSupported()),
                         BoolText(IsInternalErrorShaderName(shaderName)), pass);
      }
      return false;
    }
    int const passCount = material->get_passCount();
    if (passCount <= 0 || (pass >= 0 && pass >= passCount)) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify CB material invalid pass: material='{}' pass={} passCount={}",
                         ToStdString(material->get_name()), pass, passCount);
      }
      return false;
    }
    return true;
  }
  void SetMaterialKeyword(UnityEngine::Material* material, ::StringW keyword, bool enabled) const {
    if (!IsAlive(material)) return;
    if (enabled) {
      material->EnableKeyword(keyword);
    } else {
      material->DisableKeyword(keyword);
    }
  }
  void ApplyStereoKeywords(UnityEngine::Material* material) const {
    if (!IsAlive(material)) return;
    bool const enabled = GetMultipassRenderingEnabled();
    auto stereoMode = UnityEngine::XR::XRSettings::get_stereoRenderingMode();
    bool const singlePass = enabled && stereoMode.value__ != UnityEngine::XR::XRSettings_StereoRenderingMode::MultiPass.value__;
    bool const instanced = enabled && stereoMode.value__ == UnityEngine::XR::XRSettings_StereoRenderingMode::SinglePassInstanced.value__;
    bool const multiview = enabled && stereoMode.value__ == UnityEngine::XR::XRSettings_StereoRenderingMode::SinglePassMultiview.value__;
    bool const doubleWide = enabled && stereoMode.value__ == UnityEngine::XR::XRSettings_StereoRenderingMode::SinglePass.value__;
    SetMaterialKeyword(material, kVivifyMultipassKeyword, enabled);
    SetMaterialKeyword(material, kUnitySinglePassStereoKeyword, singlePass);
    SetMaterialKeyword(material, kUnitySinglePassInstancedKeyword, instanced);
    SetMaterialKeyword(material, kUnityStereoInstancingEnabledKeyword, instanced);
    SetMaterialKeyword(material, kUnitySinglePassMultiviewKeyword, multiview);
    SetMaterialKeyword(material, kUnityStereoMultiviewEnabledKeyword, multiview);
    SetMaterialKeyword(material, kUnityDoubleWideStereoKeyword, doubleWide);
  }
  void ReleaseDeclaredTextureData(DeclaredTextureData& data) {
    if (IsAlive(data.texture)) {
      data.texture->Release();
      UnityEngine::Object::Destroy(data.texture);
    }
    data.texture = nullptr;
  }
  void ReleaseSecondaryCameraData(SecondaryCameraData& data) {
    if (data.texturePropertyId.has_value()) {
      UnityEngine::Shader::SetGlobalTexture(data.texturePropertyId.value(), static_cast<UnityEngine::Texture*>(nullptr));
    }
    if (data.depthTexturePropertyId.has_value()) {
      UnityEngine::Shader::SetGlobalTexture(data.depthTexturePropertyId.value(), static_cast<UnityEngine::Texture*>(nullptr));
    }
    if (IsAlive(data.colorRT)) {
      data.colorRT->Release();
      UnityEngine::Object::Destroy(data.colorRT);
    }
    if (IsAlive(data.depthRT)) {
      data.depthRT->Release();
      UnityEngine::Object::Destroy(data.depthRT);
    }
    if (IsAlive(data.camera)) {
      UnityEngine::Object::Destroy(data.camera->get_gameObject());
    }
    data.colorRT = nullptr;
    data.depthRT = nullptr;
    data.camera = nullptr;
  }
  bool SameColor(UnityEngine::Color a, UnityEngine::Color b) const {
    return std::fabs(a.r - b.r) < 0.001f &&
           std::fabs(a.g - b.g) < 0.001f &&
           std::fabs(a.b - b.b) < 0.001f &&
           std::fabs(a.a - b.a) < 0.001f;
  }
  UnityEngine::Shader* FindUsableShader(std::string const& shaderName) const {
    if (shaderName.empty()) return nullptr;
    if (auto it = _supportedShadersByName.find(NormalizeAssetKey(shaderName));
        it != _supportedShadersByName.end() && IsAlive(it->second) && it->second->get_isSupported()) {
      return it->second;
    }
    auto* bundled = il2cpp_utils::try_cast<UnityEngine::Shader>(GetAssetObject(shaderName)).value_or(nullptr);
    if (IsAlive(bundled) && bundled->get_isSupported()) {
      return bundled;
    }
    auto found = UnityEngine::Shader::Find(StringW(shaderName));
    auto* foundShader = found.unsafePtr();
    if (IsAlive(foundShader) && foundShader->get_isSupported()) {
      return foundShader;
    }
    return nullptr;
  }
  UnityEngine::Shader* FindFallbackShader() const {
    static constexpr std::string_view fallbackNames[] = {
        "Unlit/Texture"sv,
        "Unlit/Color"sv,
        "Sprites/Default"sv,
        "Standard"sv,
    };
    for (auto name : fallbackNames) {
      auto shader = UnityEngine::Shader::Find(StringW(std::string(name)));
      auto* rawShader = shader.unsafePtr();
      if (IsAlive(rawShader) && rawShader->get_isSupported()) {
        return rawShader;
      }
    }
    return nullptr;
  }
  void RepairMaterialShader(UnityEngine::Material* material, std::string_view context = {}) {
    if (!IsAlive(material)) return;
    if (_repairedMaterials.contains(material)) return;
    auto shader = material->get_shader();
    auto* rawShader = shader.unsafePtr();
    auto originalShaderName = ShaderNameForLog(rawShader);
    if (GetVivifyDebugLogging() &&
        (!IsAlive(rawShader) || IsInternalErrorShaderName(originalShaderName) ||
         (IsAlive(rawShader) && !rawShader->get_isSupported()))) {
      PaperLogger.warn("Vivify shader diagnostic: context={} material='{}' shader='{}' supported={} internalError={}",
                       context,
                       ToStdString(material->get_name()),
                       originalShaderName,
                       BoolText(IsAlive(rawShader) && rawShader->get_isSupported()),
                       BoolText(IsInternalErrorShaderName(originalShaderName)));
    }
    if (IsAlive(rawShader) && rawShader->get_isSupported()) {
      ApplyStereoKeywords(material);
      _repairedMaterials.emplace(material);
      return;
    }
    UnityEngine::Shader* replacement = nullptr;
    if (IsAlive(rawShader)) {
      auto shaderName = rawShader->get_name();
      if (shaderName) {
        replacement = FindUsableShader(ToStdString(shaderName));
      }
    }
    if (!IsAlive(replacement)) {
      replacement = FindFallbackShader();
    }
    if (IsAlive(replacement)) {
      material->set_shader(replacement);
      ApplyStereoKeywords(material);
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify shader repaired: context={} material='{}' from='{}' to='{}'",
                         context, ToStdString(material->get_name()), originalShaderName, ShaderNameForLog(replacement));
      }
    } else if (GetVivifyDebugLogging()) {
      PaperLogger.warn("Vivify shader repair failed: context={} material='{}' original='{}'",
                       context, ToStdString(material->get_name()), originalShaderName);
    }
    _repairedMaterials.emplace(material);
  }
  void RepairGameObjectMaterials(UnityEngine::GameObject* gameObject, std::string_view context = {}) {
    if (!IsAlive(gameObject)) return;
    auto renderers = gameObject->GetComponentsInChildren<UnityEngine::Renderer*>(true);
    for (int i = 0; i < renderers.size(); i++) {
      auto* renderer = renderers[i];
      if (!IsAlive(renderer)) continue;
      auto materials = renderer->get_sharedMaterials();
      if (!materials) continue;
      for (int j = 0; j < materials.size(); j++) {
        RepairMaterialShader(materials[j].unsafePtr(), context);
      }
    }
  }
  void ApplyGameObjectStereoKeywords(UnityEngine::GameObject* gameObject) {
    if (!IsAlive(gameObject)) return;
    auto renderers = gameObject->GetComponentsInChildren<UnityEngine::Renderer*>(true);
    for (int i = 0; i < renderers.size(); i++) {
      auto* renderer = renderers[i];
      if (!IsAlive(renderer)) continue;
      auto materials = renderer->get_sharedMaterials();
      if (!materials) continue;
      for (int j = 0; j < materials.size(); j++) {
        ApplyStereoKeywords(materials[j].unsafePtr());
      }
    }
  }
  void RefreshLoadedMaterialStereoKeywords() {
    for (auto const& [_, asset] : _assets) {
      if (!IsAlive(asset)) continue;
      if (auto* material = il2cpp_utils::try_cast<UnityEngine::Material>(asset).value_or(nullptr); IsAlive(material)) {
        ApplyStereoKeywords(material);
      } else if (auto* gameObject = il2cpp_utils::try_cast<UnityEngine::GameObject>(asset).value_or(nullptr); IsAlive(gameObject)) {
        ApplyGameObjectStereoKeywords(gameObject);
      }
    }
  }
  void RepairLoadedMaterialShaders() {
    for (auto const& [_, asset] : _assets) {
      if (!IsAlive(asset)) continue;
      if (auto* material = il2cpp_utils::try_cast<UnityEngine::Material>(asset).value_or(nullptr); IsAlive(material)) {
        RepairMaterialShader(material, _);
      } else if (auto* gameObject = il2cpp_utils::try_cast<UnityEngine::GameObject>(asset).value_or(nullptr); IsAlive(gameObject)) {
        RepairGameObjectMaterials(gameObject, _);
      }
    }
  }
  bool AssignmentMatchesTracks(AssignedPrefabInfo const& info, GlobalNamespace::NoteData* noteData) {
    if (info.tracks.empty()) return true;
    if (noteData == nullptr) return false;
    auto* customNoteData = il2cpp_utils::try_cast<CustomJSONData::CustomNoteData>(noteData).value_or(nullptr);
    if (customNoteData == nullptr || customNoteData->customData == nullptr) return false;
    auto& ad = TracksAD::getAD(customNoteData->customData);
    std::unordered_set<TrackW> assignedSet(info.tracks.begin(), info.tracks.end());
    auto matches = [&](auto const& noteTracks) {
      for (auto const& noteTrack : noteTracks) {
        if (assignedSet.count(noteTrack)) return true;
      }
      return false;
    };
    if (matches(ad.tracks)) return true;
    if (customNoteData->customData->value.has_value()) {
      bool const v2 = _currentBeatmapData != nullptr && _currentBeatmapData->v2orEarlier;
      auto parsedTracks = ReadTracks(customNoteData->customData->value.value().get(), v2);
      if (matches(parsedTracks)) return true;
    }
    return false;
  }
  void AddAssignedPrefab(std::string_view objectType,
                         AssignedPrefabKind kind,
                         std::string asset,
                         std::vector<TrackW> tracks,
                         bool additive,
                         std::optional<int> saberType = std::nullopt) {
    if (asset.empty()) return;
    // NOTE: Do NOT pre-validate the asset here with GetAssetAs. That causes a
    // silent drop whenever the asset key doesn't resolve at event-fire time (e.g.
    // short name vs full Unity path mismatch, or transient bundle state). The
    // info is never retried and notes stay stock for the entire song.
    // GetValidPrefabInfos() re-validates at actual spawn time, which is correct.
    AssignedPrefabInfo info;
    info.asset = std::move(asset);
    info.tracks = std::move(tracks);
    info.objectType = std::string(objectType);
    info.saberType = saberType;
    info.kind = kind;
    info.additive = additive;
    _assignedPrefabs.push_back(std::move(info));
  }
  void AddAssignedTrail(std::string asset,
                        bool additive,
                        int saberType,
                        std::optional<UnityEngine::Vector3> topPos,
                        std::optional<UnityEngine::Vector3> bottomPos,
                        std::optional<float> duration,
                        std::optional<int> samplingFrequency,
                        std::optional<int> granularity) {
    if (asset.empty()) return;
    auto* material = GetAssetAs<UnityEngine::Material>(asset);
    if (material == nullptr || !UnityEngine::Object::op_Implicit_bool(material)) return;
    AssignedPrefabInfo info;
    info.asset = std::move(asset);
    info.objectType = "saber";
    info.saberType = saberType;
    info.kind = AssignedPrefabKind::Trail;
    info.additive = additive;
    info.trailTopPos = topPos;
    info.trailBottomPos = bottomPos;
    info.trailDuration = duration;
    info.trailSamplingFrequency = samplingFrequency;
    info.trailGranularity = granularity;
    _assignedPrefabs.push_back(std::move(info));
  }
  bool TracksOverlap(std::vector<TrackW> const& left, std::vector<TrackW> const& right) const {
    if (left.empty() || right.empty()) return false;
    for (auto const& leftTrack : left) {
      for (auto const& rightTrack : right) {
        if (leftTrack == rightTrack) return true;
      }
    }
    return false;
  }
  void ClearAssignedPrefabs(std::string_view objectType,
                            std::optional<AssignedPrefabKind> kind = std::nullopt,
                            std::optional<int> saberType = std::nullopt,
                            std::vector<TrackW> const* tracks = nullptr) {
    _assignedPrefabs.erase(std::remove_if(_assignedPrefabs.begin(), _assignedPrefabs.end(),
        [this, objectType, kind, saberType, tracks](AssignedPrefabInfo const& info) {
          if (info.objectType != objectType) return false;
          if (kind.has_value() && info.kind != kind.value()) return false;
          if (saberType.has_value() && (!info.saberType.has_value() || info.saberType.value() != saberType.value())) return false;
          if (tracks != nullptr && !tracks->empty() && !info.tracks.empty() && !TracksOverlap(info.tracks, *tracks)) {
            return false;
          }
          return true;
        }),
        _assignedPrefabs.end());
  }
  std::vector<AssignedPrefabInfo*> GetValidPrefabInfos(std::vector<AssignedPrefabInfo*> const& infos) {
    std::vector<AssignedPrefabInfo*> result;
    result.reserve(infos.size());
    for (auto* info : infos) {
      if (info == nullptr) continue;
      bool valid = false;
      if (info->kind == AssignedPrefabKind::Trail) {
        auto* material = GetAssetAs<UnityEngine::Material>(info->asset);
        valid = material != nullptr && UnityEngine::Object::op_Implicit_bool(material);
      } else {
        auto* prefab = GetAssetAs<UnityEngine::GameObject>(info->asset);
        valid = prefab != nullptr && UnityEngine::Object::op_Implicit_bool(prefab);
      }
      if (valid) {
        result.emplace_back(info);
      }
    }
    return result;
  }
  bool ShouldHideOriginal(std::vector<AssignedPrefabInfo*> const& infos) const {
    return std::any_of(infos.begin(), infos.end(), [](AssignedPrefabInfo const* info) {
      return info != nullptr && !info->additive;
    });
  }
  void DisableOriginalRenderers(UnityEngine::GameObject* gameObject, VisualReplacement& replacement) {
    if (!IsAlive(gameObject)) return;
    auto renderers = gameObject->GetComponentsInChildren<UnityEngine::Renderer*>(true);
    for (int i = 0; i < renderers.size(); i++) {
      auto* renderer = renderers[i];
      if (IsAlive(renderer) && renderer->get_enabled()) {
        renderer->set_enabled(false);
        replacement.disabledRenderers.emplace_back(renderer);
      }
    }
  }
  void DisableOriginalRenderers(ArrayW<UnityEngine::Renderer*, Array<UnityEngine::Renderer*>*> const& renderers,
                                VisualReplacement& replacement) {
    if (!renderers) return;
    for (int i = 0; i < renderers.size(); i++) {
      auto* renderer = renderers[i];
      if (IsAlive(renderer) && renderer->get_enabled()) {
        renderer->set_enabled(false);
        replacement.disabledRenderers.emplace_back(renderer);
      }
    }
  }
  UnityEngine::Transform* GetReplacementParent(GlobalNamespace::NoteController* noteController) {
    if (!IsAlive(noteController)) return nullptr;
    // Use the raw ____noteTransform backing field (offset 0x28), NOT GetChild(0).
    // GetChild(0) is wrong: NoteMovement sits at child index 0 of the root
    // MonoBehaviour transform, so the replacement prefab gets parented inside
    // NoteMovement's subtree and becomes invisible/mispositioned.
    // ____noteTransform is the actual visual transform set by GameNoteController::Init.
    auto noteTransform = noteController->____noteTransform;
    if (IsManagedAlive(noteTransform)) {
      auto* noteTransformPtr = noteTransform.unsafePtr();
      if (IsAlive(noteTransformPtr)) return noteTransformPtr;
    }
    // Fallback to root if ____noteTransform is unset (e.g. BombNoteController
    // may not populate it; its root IS the visual transform).
    auto rootTransform = noteController->get_transform();
    auto* rootTransformPtr = SafeUnityPtr(rootTransform);
    return IsAlive(rootTransformPtr) ? rootTransformPtr : nullptr;
  }
  GlobalNamespace::MaterialPropertyBlockController* GetReplacementMaterialPropertyBlockController(
      GlobalNamespace::NoteController* noteController, UnityEngine::Transform* replacementParent) {
    if (!IsAlive(noteController)) return nullptr;
    // replacementParent is now always ____noteTransform (the correct visual
    // transform), so we can check it unconditionally — no need to gate on
    // UsesNoteVisualChild. The old gate meant BombNoteController always fell
    // through to the slower component searches even when the parent was valid.
    if (IsAlive(replacementParent)) {
      auto parentObject = replacementParent->get_gameObject();
      if (IsManagedAlive(parentObject)) {
        auto* parentGO = parentObject.unsafePtr();
        if (IsAlive(parentGO)) {
          auto* childMpb = parentGO->GetComponent<GlobalNamespace::MaterialPropertyBlockController*>();
          if (IsAlive(childMpb)) return childMpb;
        }
      }
    }
    auto* controllerMpb = noteController->GetComponent<GlobalNamespace::MaterialPropertyBlockController*>();
    if (IsAlive(controllerMpb)) return controllerMpb;
    return noteController->GetComponentInChildren<GlobalNamespace::MaterialPropertyBlockController*>(true);
  }
  void RestoreReplacementData(VisualReplacement& replacement) {
    if (replacement.hasOriginalMaterialBlockRenderers && IsAlive(replacement.materialPropertyBlockController)) {
      if (replacement.originalMaterialBlockRenderers) {
        std::vector<UnityEngine::Renderer*> aliveRenderers;
        aliveRenderers.reserve(replacement.originalMaterialBlockRenderers.size());
        for (int i = 0; i < replacement.originalMaterialBlockRenderers.size(); i++) {
          auto originalRenderer = replacement.originalMaterialBlockRenderers[i];
          auto* renderer = originalRenderer ? originalRenderer.unsafePtr() : nullptr;
          if (IsAlive(renderer)) {
            aliveRenderers.emplace_back(renderer);
          }
        }
        auto restoredRenderers = ArrayW<UnityW<UnityEngine::Renderer>>(aliveRenderers.size());
        for (int i = 0; i < aliveRenderers.size(); i++) {
          restoredRenderers[i] = aliveRenderers[i];
        }
        replacement.materialPropertyBlockController->____renderers = restoredRenderers;
      } else {
        replacement.materialPropertyBlockController->____renderers = ArrayW<UnityW<UnityEngine::Renderer>>(static_cast<il2cpp_array_size_t>(0));
      }
      ApplyMaterialPropertyBlockChanges(replacement.materialPropertyBlockController, "RestoreReplacementData");
    }
    for (auto* followedTrail : replacement.followedTrails) {
      if (IsAlive(followedTrail)) {
        followedTrail->Cleanup();
      }
    }
    for (auto* renderer : replacement.disabledRenderers) {
      if (IsAlive(renderer)) {
        renderer->set_enabled(true);
      }
    }
    for (auto* spawned : replacement.spawnedObjects) {
      if (IsAlive(spawned)) {
        UnityEngine::Object::Destroy(spawned);
      }
    }
    if (replacement.saberColorPropertyBlock != nullptr) {
      replacement.saberColorPropertyBlock->Dispose();
      replacement.saberColorPropertyBlock = nullptr;
    }
    replacement.disabledRenderers.clear();
    replacement.spawnedObjects.clear();
    replacement.replacementRenderers.clear();
    replacement.followedTrails.clear();
    replacement.materialPropertyBlockController = nullptr;
    replacement.originalMaterialBlockRenderers = nullptr;
    replacement.hasOriginalMaterialBlockRenderers = false;
    replacement.hasLastSaberColor = false;
  }
  void CacheReplacementRenderers(UnityEngine::GameObject* spawned, VisualReplacement& replacement) {
    if (!IsAlive(spawned)) return;
    auto renderers = spawned->GetComponentsInChildren<UnityEngine::Renderer*>(true);
    replacement.replacementRenderers.reserve(replacement.replacementRenderers.size() + renderers.size());
    for (int i = 0; i < renderers.size(); i++) {
      auto* renderer = renderers[i];
      if (IsAlive(renderer)) {
        ForceRendererOnTop(renderer);
        replacement.replacementRenderers.emplace_back(renderer);
      }
    }
  }
  void InstantiateReplacementPrefab(AssignedPrefabInfo const& info,
                                    UnityEngine::Transform* parent,
                                    VisualReplacement& replacement,
                                    int overrideLayer = -1) {
    if (!IsAlive(parent)) return;
    auto* prefab = GetAssetAs<UnityEngine::GameObject>(info.asset);
    if (prefab == nullptr || !UnityEngine::Object::op_Implicit_bool(prefab)) return;
    auto* spawned = UnityEngine::Object::Instantiate(prefab);
    if (!IsAlive(spawned)) return;
    CleanCustomObject(spawned);
    RepairGameObjectMaterials(spawned, info.asset);
    auto* spawnedTransform = SafeUnityPtr(spawned->get_transform());
    if (!IsAlive(spawnedTransform)) {
      UnityEngine::Object::Destroy(spawned);
      return;
    }
    spawnedTransform->SetParent(parent, false);
    auto parentObject = parent->get_gameObject();
    if (overrideLayer >= 0) {
      SetLayerRecursively(spawned, overrideLayer);
    } else if (IsManagedAlive(parentObject)) {
      auto* parentGO = parentObject.unsafePtr();
      if (IsAlive(parentGO)) {
        SetLayerRecursively(spawned, parentGO->get_layer());
      }
    }
    CleanCustomObject(spawned);
    if (overrideLayer >= 0) {
      SetLayerRecursively(spawned, overrideLayer);
    }
    replacement.spawnedObjects.emplace_back(spawned);
    CacheReplacementRenderers(spawned, replacement);
  }
  void SanitizeActiveNoteReplacements() {
    if (_noteReplacements.empty()) return;
    for (auto it = _noteReplacements.begin(); it != _noteReplacements.end();) {
      if (!IsAlive(it->first)) {
        RestoreReplacementData(it->second);
        it = _noteReplacements.erase(it);
        continue;
      }
      for (auto* spawned : it->second.spawnedObjects) {
        if (!IsAlive(spawned)) continue;
        CleanCustomObject(spawned);
        SetLayerRecursively(spawned, kVisualOnlyLayer);
      }
      ++it;
    }
  }
  UnityEngine::Color GetSaberColor(GlobalNamespace::SaberModelController* smc, GlobalNamespace::Saber* saber) {
    auto saberType = TryGetSaberType(saber);
    if (IsAlive(smc) && smc->____colorManager != nullptr && saberType.has_value()) {
      return smc->____colorManager->ColorForSaberType(GlobalNamespace::SaberType(saberType.value()));
    }
    return saberType.value_or(GlobalNamespace::SaberType::SaberB.value__) == GlobalNamespace::SaberType::SaberA.value__
        ? UnityEngine::Color(1.0f, 0.0f, 0.0f, 1.0f)
        : UnityEngine::Color(0.0f, 0.55f, 1.0f, 1.0f);
  }
  void ApplySaberReplacementColor(GlobalNamespace::SaberModelController* smc,
                                  GlobalNamespace::Saber* saber,
                                  VisualReplacement& replacement,
                                  bool force = false) {
    if (!IsAlive(smc) || !IsAlive(saber) || replacement.spawnedObjects.empty()) return;
    auto color = GetSaberColor(smc, saber);
    if (!force && replacement.hasLastSaberColor && SameColor(color, replacement.lastSaberColor)) {
      return;
    }
    if (replacement.saberColorPropertyBlock == nullptr) {
      replacement.saberColorPropertyBlock = UnityEngine::MaterialPropertyBlock::New_ctor();
    }
    replacement.saberColorPropertyBlock->SetColor(ColorPropertyId(), color);
    for (auto* renderer : replacement.replacementRenderers) {
      if (IsAlive(renderer)) {
        renderer->SetPropertyBlock(replacement.saberColorPropertyBlock);
      }
    }
    replacement.lastSaberColor = color;
    replacement.hasLastSaberColor = true;
  }
  void UpdateSaberReplacementColors() {
    if (_saberReplacements.empty() || _currentBeatmapData == nullptr || _isResetting) return;
    PurgeInvalidActiveSabers();
    for (auto const& target : _activeSabers) {
      auto it = _saberReplacements.find(target.controller);
      if (it != _saberReplacements.end()) {
        ApplySaberReplacementColor(target.controller, target.saber, it->second);
      }
    }
  }
  void ApplySaberTrailVisuals(GlobalNamespace::SaberModelController* smc,
                              std::vector<AssignedPrefabInfo*> const& infos,
                              VisualReplacement& replacement) {
    if (infos.empty() || !IsAlive(smc)) return;
    auto* sourceTrail = SafeUnityPtr(smc->____saberTrail);
    if (!IsAlive(sourceTrail) || !sourceTrail->____trailRendererPrefab) return;
    UnityEngine::Transform* parent = nullptr;
    auto existing = std::find_if(_activeSabers.begin(), _activeSabers.end(), [smc](ActiveSaberVisual const& target) {
      return target.controller == smc;
    });
    if (existing != _activeSabers.end()) {
      parent = existing->parent;
    }
    if (!IsAlive(parent)) {
      auto transform = smc->get_transform();
      parent = SafeUnityPtr(transform);
    }
    if (!IsAlive(parent)) return;

    auto const defaultTop = UnityEngine::Vector3(0.0f, 0.0f, 1.0f);
    auto const defaultBottom = UnityEngine::Vector3(0.0f, 0.0f, 0.0f);
    for (auto* info : infos) {
      if (info == nullptr) continue;
      auto* material = GetAssetAs<UnityEngine::Material>(info->asset);
      if (material == nullptr || !UnityEngine::Object::op_Implicit_bool(material)) continue;
      RepairMaterialShader(material, info->asset);

      auto top = info->trailTopPos.value_or(defaultTop);
      auto bottom = info->trailBottomPos.value_or(defaultBottom);
      float duration = info->trailDuration.value_or(0.4f);
      if (!std::isfinite(duration)) duration = 0.4f;
      duration = std::clamp(duration, 0.01f, kMaxTrailDuration);
      int samplingFrequency = info->trailSamplingFrequency.value_or(50);
      samplingFrequency = std::clamp(samplingFrequency, 1, kMaxTrailSamplingFrequency);
      int granularity = info->trailGranularity.value_or(60);
      granularity = std::clamp(granularity, 1, kMaxTrailGranularity);

      auto* trailObject = UnityEngine::GameObject::New_ctor(u"VivifyFollowedSaberTrail");
      if (!IsAlive(trailObject)) continue;
      auto* trailTransform = SafeUnityPtr(trailObject->get_transform());
      if (!IsAlive(trailTransform)) {
        UnityEngine::Object::Destroy(trailObject);
        continue;
      }
      trailTransform->SetParent(parent, false);
      if (auto* parentGO = SafeUnityPtr(parent->get_gameObject()); IsAlive(parentGO)) {
        SetLayerRecursively(trailObject, parentGO->get_layer());
      }
      auto* followedTrail = trailObject->AddComponent<FollowedSaberTrail*>();
      if (!IsAlive(followedTrail)) {
        UnityEngine::Object::Destroy(trailObject);
        continue;
      }
      followedTrail->InitFollowed(sourceTrail, parent, material, top, bottom, duration, samplingFrequency, granularity);
      auto* followedRenderer = SafeUnityPtr(followedTrail->____trailRenderer);
      if (!IsAlive(followedRenderer)) {
        UnityEngine::Object::Destroy(trailObject);
        continue;
      }
      if (IsAlive(followedRenderer) && followedRenderer->____meshRenderer) {
        if (auto* parentGO = SafeUnityPtr(parent->get_gameObject()); IsAlive(parentGO)) {
          auto* trailRendererGO = SafeUnityPtr(followedRenderer->get_gameObject());
          if (IsAlive(trailRendererGO)) {
            SetLayerRecursively(trailRendererGO, parentGO->get_layer());
          }
        }
        ForceRendererOnTop(followedRenderer->____meshRenderer);
      }
      replacement.spawnedObjects.emplace_back(trailObject);
      replacement.followedTrails.emplace_back(followedTrail);
    }

    if (!replacement.followedTrails.empty() && ShouldHideOriginal(infos)) {
      auto* sourceRenderer = SafeUnityPtr(sourceTrail->____trailRenderer);
      if (IsAlive(sourceRenderer) && sourceRenderer->____meshRenderer && sourceRenderer->____meshRenderer->get_enabled()) {
        sourceRenderer->____meshRenderer->set_enabled(false);
        replacement.disabledRenderers.emplace_back(sourceRenderer->____meshRenderer);
      }
    }
  }
  void ApplyReplacementRenderersToMaterialBlock(GlobalNamespace::MaterialPropertyBlockController* mpb,
                                                VisualReplacement& replacement,
                                                bool hideOriginal) {
    if (!IsAlive(mpb)) return;
    std::vector<UnityEngine::Renderer*> replacementRenderers;
    replacementRenderers.reserve(replacement.replacementRenderers.size());
    for (auto* renderer : replacement.replacementRenderers) {
      if (IsAlive(renderer)) {
        replacementRenderers.emplace_back(renderer);
      }
    }
    if (replacementRenderers.empty()) return;
    if (!replacement.hasOriginalMaterialBlockRenderers) {
      replacement.materialPropertyBlockController = mpb;
      replacement.originalMaterialBlockRenderers = mpb->____renderers;
      replacement.hasOriginalMaterialBlockRenderers = true;
    }

    std::vector<UnityEngine::Renderer*> originalRenderers;
    if (!hideOriginal && replacement.originalMaterialBlockRenderers) {
      originalRenderers.reserve(replacement.originalMaterialBlockRenderers.size());
      for (int i = 0; i < replacement.originalMaterialBlockRenderers.size(); i++) {
        auto originalRenderer = replacement.originalMaterialBlockRenderers[i];
        auto* renderer = originalRenderer ? originalRenderer.unsafePtr() : nullptr;
        if (IsAlive(renderer)) {
          originalRenderers.emplace_back(renderer);
        }
      }
    }
    auto convertedRenderers = ArrayW<UnityW<UnityEngine::Renderer>>(originalRenderers.size() + replacementRenderers.size());
    for (int i = 0; i < originalRenderers.size(); i++) {
      convertedRenderers[i] = originalRenderers[i];
    }
    for (int i = 0; i < replacementRenderers.size(); i++) {
      convertedRenderers[originalRenderers.size() + i] = replacementRenderers[i];
    }
    mpb->____renderers = convertedRenderers;
    ApplyMaterialPropertyBlockChanges(mpb, "ApplyReplacementRenderersToMaterialBlock");
  }
  void ApplyReplacementRenderersToMaterialBlock(UnityEngine::GameObject* gameObject,
                                                VisualReplacement& replacement,
                                                bool hideOriginal) {
    if (!IsAlive(gameObject)) return;
    auto* mpb = gameObject->GetComponentInChildren<GlobalNamespace::MaterialPropertyBlockController*>(true);
    if (IsAlive(mpb)) {
      ApplyReplacementRenderersToMaterialBlock(mpb, replacement, hideOriginal);
    }
  }
  void RestoreAllVisualReplacements() {
    for (auto& [_, replacement] : _noteReplacements) RestoreReplacementData(replacement);
    _noteReplacements.clear();
    for (auto& [_, replacement] : _saberReplacements) RestoreReplacementData(replacement);
    _saberReplacements.clear();
    for (auto& [_, replacement] : _debrisReplacements) RestoreReplacementData(replacement);
    _debrisReplacements.clear();
  }
  void PurgeInvalidActiveSabers() {
    _activeSabers.erase(std::remove_if(_activeSabers.begin(), _activeSabers.end(), [this](ActiveSaberVisual const& target) {
      return !IsAlive(target.controller) || !IsAlive(target.saber) || !IsAlive(target.parent);
    }), _activeSabers.end());
  }
  void PreloadInstantiatePrefabs() {
    if (_currentBeatmapData == nullptr) {
      return;
    }
    bool const v2 = _currentBeatmapData->v2orEarlier;
    std::unordered_set<std::string> seenIds;
    for (auto* customEventData : _currentBeatmapData->customEventDatas) {
      if (customEventData == nullptr || customEventData->type != kInstantiatePrefabEvent) {
        continue;
      }
      auto* json = GetEventJson(customEventData);
      if (json == nullptr) {
        continue;
      }
      auto asset = ReadStringView(*json, "asset");
      if (!asset.has_value()) {
        continue;
      }
      InstantiatePrefabData data;
      data.asset = std::string(*asset);
      if (auto id = ReadStringView(*json, "id"); id.has_value()) {
        data.id = std::string(*id);
        if (!seenIds.emplace(*data.id).second) {
        }
      }
      data.transformData = Tracks::TransformData(*json, v2);
      data.tracks = ReadTracks(*json, v2);
      if (auto* prefab = GetAssetAs<UnityEngine::GameObject>(data.asset); prefab != nullptr) {
        data.instance = UnityEngine::Object::Instantiate(prefab);
        data.instance->SetActive(false);
      }
      _instantiatePrefabs.emplace(customEventData, std::move(data));
    }
  }
  std::string GetPrefabStorageId(CustomJSONData::CustomEventData* customEventData,
                                 InstantiatePrefabData const& data) const {
    if (data.id.has_value()) {
      return *data.id;
    }
    return "vivify_prefab_" + std::to_string(reinterpret_cast<std::uintptr_t>(customEventData));
  }
  void InstantiatePrefab(CustomJSONData::CustomEventData* customEventData, rapidjson::Value const&) {
    auto it = _instantiatePrefabs.find(customEventData);
    if (it == _instantiatePrefabs.end()) {
      return;
    }
    auto& data = it->second;
    if (data.instance == nullptr) {
      auto* prefab = GetAssetAs<UnityEngine::GameObject>(data.asset);
      if (prefab == nullptr) {
        return;
      }
      data.instance = UnityEngine::Object::Instantiate(prefab);
      data.instance->SetActive(false);
    }
    std::string storageId = GetPrefabStorageId(customEventData, data);
    if (_livePrefabs.contains(storageId)) {
      DestroyPrefabById(storageId);
    }
    auto* instance = data.instance;
    instance->SetActive(true);
    auto transform = instance->get_transform();
    bool const v2 = _currentBeatmapData != nullptr && _currentBeatmapData->v2orEarlier;
    data.transformData.Apply(transform, false, v2);
    if (!data.tracks.empty()) {
      for (auto const& track : data.tracks) {
        track.RegisterGameObject(instance);
      }
      auto tracksSpan = std::span<TrackW const>(data.tracks.data(), data.tracks.size());
      Tracks::GameObjectTrackController::HandleTrackData(
          instance,
          tracksSpan,
          GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance,
          v2,
          false);
      ApplyTrackCameraCullingToGameObject(instance, data.tracks);
    }
    auto animatorArray = instance->GetComponentsInChildren<UnityEngine::Animator*>(true);
    std::vector<UnityEngine::Animator*> animators;
    animators.reserve(animatorArray.size());
    for (auto animator : animatorArray) {
      if (animator != nullptr) {
        animators.emplace_back(animator);
      }
    }
    _livePrefabs[storageId] = LivePrefab{
        .gameObject = instance,
        .tracks = data.tracks,
        .animators = std::move(animators),
    };

    // Immediately rebuild overlay masks after prefab registration.
    // Some Quest Vivify maps (such as 42-flux) rely on this state
    // before pause/resume events occur.
    RebuildGameplayOverlayMaskFromLivePrefabs();
    RefreshCameraComponents(true);
  }
  bool DestroyPrefabById(std::string const& id) {
    auto it = _livePrefabs.find(id);
    if (it == _livePrefabs.end()) {
      return false;
    }
    auto prefab = std::move(it->second);
    _livePrefabs.erase(it);
    if (prefab.gameObject != nullptr) {
      for (auto const& track : prefab.tracks) {
        track.UnregisterGameObject(prefab.gameObject);
      }
      UnityEngine::Object::Destroy(prefab.gameObject);
    }
    return true;
  }
  void DestroyObjects(rapidjson::Value const& json) {
    for (auto const& id : ReadStringListOrSingle(json, "id")) {
      if (!DestroyPrefabById(id)) {
      }
    }
  }
  float CurrentSongTime() {
    // Cache the result per-frame: during a single Update() tick this function is
    // called 6+ times (UpdateMaterialAnimations, UpdateGlobalAnimations,
    // UpdateAnimatorAnimations, UpdateBlitEffects, UpdateRenderSettingAnimations,
    // DetectSongRestart). Each call normally goes through il2cpp to
    // _audioTimeSyncController->get_songTime(). Caching by frame number reduces
    // this to one il2cpp call per frame while keeping event handlers (which run
    // on a different cadence) always getting a fresh value.
    int const frame = static_cast<int>(UnityEngine::Time::get_frameCount());
    if (frame == _songTimeCacheFrame) return _songTimeCache;
    _songTimeCacheFrame = frame;
    if (_audioTimeSyncController == nullptr) {
      _audioTimeSyncController = UnityEngine::Object::FindObjectOfType<GlobalNamespace::AudioTimeSyncController*>();
    }
    _songTimeCache = _audioTimeSyncController != nullptr ? _audioTimeSyncController->get_songTime() : 0.0f;
    return _songTimeCache;
  }
  void DetectSongRestart() {
    if (_currentBeatmapData == nullptr || _isResetting) return;
    float songTime = CurrentSongTime();
    if (_lastSongTime >= 0.0f && songTime + 0.25f < _lastSongTime) {
      ResetRuntime();
      return;
    }
    _lastSongTime = songTime;
  }
  float CurrentBpm() const {
    if (TracksStatic::bpmController) {
      return TracksStatic::bpmController->get_currentBpm();
    }
    return 0.0f;
  }
  float DurationBeatsToSeconds(float durationBeats) const {
    float bpm = CurrentBpm();
    if (bpm <= 0.0f) {
      return 0.0f;
    }
    return (60.0f * durationBeats) / bpm;
  }
  std::optional<MaterialPropertyChange> ParseMaterialProperty(rapidjson::Value const& propertyJson) {
    auto id = ReadStringView(propertyJson, "id");
    auto type = ReadStringView(propertyJson, "type");
    if (!id.has_value() || !type.has_value()) {
      return std::nullopt;
    }
    MaterialPropertyChange property;
    property.kind = ParseMaterialPropertyKind(*type);
    if (property.kind == MaterialPropertyKind::Keyword) {
      property.id = std::string(*id);
    } else {
      property.id = UnityEngine::Shader::PropertyToID(StringW(*id));
    }
    switch (property.kind) {
      case MaterialPropertyKind::Texture: {
        auto texture = ReadStringView(propertyJson, "value");
        if (!texture.has_value()) {
          return std::nullopt;
        }
        property.value = std::string(*texture);
        return property;
      }
      case MaterialPropertyKind::Color: {
        auto point = MakePointDefinition(propertyJson, "value", Tracks::ffi::WrapBaseValueType::Vec4);
        if (!point.has_value()) {
          return std::nullopt;
        }
        property.value = *point;
        return property;
      }
      case MaterialPropertyKind::Float: {
        if (auto point = MakePointDefinition(propertyJson, "value", Tracks::ffi::WrapBaseValueType::Float); point.has_value()) {
          property.value = *point;
        } else if (auto value = ReadFloat(propertyJson, "value"); value.has_value()) {
          property.value = *value;
        } else {
          return std::nullopt;
        }
        return property;
      }
      case MaterialPropertyKind::Int: {
        if (auto value = ReadInt(propertyJson, "value"); value.has_value()) {
          property.value = *value;
          return property;
        }
        return std::nullopt;
      }
      case MaterialPropertyKind::Vector: {
        auto point = MakePointDefinition(propertyJson, "value", Tracks::ffi::WrapBaseValueType::Vec4);
        if (!point.has_value()) {
          return std::nullopt;
        }
        property.value = *point;
        return property;
      }
      case MaterialPropertyKind::Keyword: {
        if (auto point = MakePointDefinition(propertyJson, "value", Tracks::ffi::WrapBaseValueType::Float); point.has_value()) {
          property.value = *point;
        } else if (auto value = ReadBool(propertyJson, "value"); value.has_value()) {
          property.value = *value;
        } else {
          return std::nullopt;
        }
        return property;
      }
      case MaterialPropertyKind::Unsupported:
      default:
        return std::nullopt;
    }
  }
  std::vector<MaterialPropertyChange> ParseMaterialProperties(rapidjson::Value const& json) {
    std::vector<MaterialPropertyChange> properties;
    auto* propertiesValue = ReadValuePtr(json, "properties");
    if (propertiesValue == nullptr || !propertiesValue->IsArray()) {
      return properties;
    }
    properties.reserve(propertiesValue->Size());
    for (auto const& propertyJson : propertiesValue->GetArray()) {
      if (!propertyJson.IsObject()) {
        continue;
      }
      if (auto property = ParseMaterialProperty(propertyJson); property.has_value()) {
        properties.emplace_back(std::move(*property));
      }
    }
    return properties;
  }
  std::optional<AnimatorPropertyChange> ParseAnimatorProperty(rapidjson::Value const& propertyJson) {
    auto name = ReadStringView(propertyJson, "id");
    auto type = ReadStringView(propertyJson, "type");
    if (!name.has_value() || !type.has_value()) {
      return std::nullopt;
    }
    AnimatorPropertyChange property;
    property.name = std::string(*name);
    property.kind = ParseAnimatorPropertyKind(*type);
    switch (property.kind) {
      case AnimatorPropertyKind::Bool: {
        if (auto point = MakePointDefinition(propertyJson, "value", Tracks::ffi::WrapBaseValueType::Float); point.has_value()) {
          property.value = *point;
        } else if (auto value = ReadBool(propertyJson, "value"); value.has_value()) {
          property.value = *value;
        } else {
          return std::nullopt;
        }
        return property;
      }
      case AnimatorPropertyKind::Float: {
        if (auto point = MakePointDefinition(propertyJson, "value", Tracks::ffi::WrapBaseValueType::Float); point.has_value()) {
          property.value = *point;
        } else if (auto value = ReadFloat(propertyJson, "value"); value.has_value()) {
          property.value = *value;
        } else {
          return std::nullopt;
        }
        return property;
      }
      case AnimatorPropertyKind::Integer: {
        if (auto point = MakePointDefinition(propertyJson, "value", Tracks::ffi::WrapBaseValueType::Float); point.has_value()) {
          property.value = *point;
        } else if (auto value = ReadInt(propertyJson, "value"); value.has_value()) {
          property.value = *value;
        } else {
          return std::nullopt;
        }
        return property;
      }
      case AnimatorPropertyKind::Trigger: {
        property.value = ReadBool(propertyJson, "value").value_or(true);
        return property;
      }
      case AnimatorPropertyKind::Unsupported:
      default:
        return std::nullopt;
    }
  }
  std::vector<AnimatorPropertyChange> ParseAnimatorProperties(rapidjson::Value const& json) {
    std::vector<AnimatorPropertyChange> properties;
    auto* propertiesValue = ReadValuePtr(json, "properties");
    if (propertiesValue == nullptr || !propertiesValue->IsArray()) {
      return properties;
    }
    properties.reserve(propertiesValue->Size());
    for (auto const& propertyJson : propertiesValue->GetArray()) {
      if (!propertyJson.IsObject()) {
        continue;
      }
      if (auto property = ParseAnimatorProperty(propertyJson); property.has_value()) {
        properties.emplace_back(std::move(*property));
      }
    }
    return properties;
  }
  bool IsAnimated(MaterialPropertyChange const& property) const {
    return std::holds_alternative<PointDefinitionW>(property.value);
  }
  bool IsAnimated(AnimatorPropertyChange const& property) const {
    return std::holds_alternative<PointDefinitionW>(property.value);
  }
  void ApplyMaterialProperty(UnityEngine::Material* material, MaterialPropertyChange const& property, float progress) {
    if (material == nullptr) {
      return;
    }
    switch (property.kind) {
      case MaterialPropertyKind::Texture: {
        auto propertyId = std::get<int>(property.id);
        auto* texture = GetAssetAs<UnityEngine::Texture>(std::get<std::string>(property.value));
        if (texture != nullptr) {
          material->SetTexture(propertyId, texture);
        }
        break;
      }
      case MaterialPropertyKind::Color: {
        auto propertyId = std::get<int>(property.id);
        auto const& point = std::get<PointDefinitionW>(property.value);
        material->SetColor(propertyId, VectorToColor(point.InterpolateVector4(progress)));
        break;
      }
      case MaterialPropertyKind::Float: {
        auto propertyId = std::get<int>(property.id);
        if (std::holds_alternative<PointDefinitionW>(property.value)) {
          material->SetFloat(propertyId, std::get<PointDefinitionW>(property.value).InterpolateLinear(progress));
        } else {
          material->SetFloat(propertyId, std::get<float>(property.value));
        }
        break;
      }
      case MaterialPropertyKind::Int: {
        auto propertyId = std::get<int>(property.id);
        material->SetInt(propertyId, std::get<int>(property.value));
        break;
      }
      case MaterialPropertyKind::Vector: {
        auto propertyId = std::get<int>(property.id);
        auto const& point = std::get<PointDefinitionW>(property.value);
        material->SetVector(propertyId, ToUnityVector(point.InterpolateVector4(progress)));
        break;
      }
      case MaterialPropertyKind::Keyword: {
        auto const& keyword = std::get<std::string>(property.id);
        bool enabled = false;
        if (std::holds_alternative<PointDefinitionW>(property.value)) {
          enabled = std::get<PointDefinitionW>(property.value).InterpolateLinear(progress) >= 1.0f;
        } else {
          enabled = std::get<bool>(property.value);
        }
        if (enabled) {
          material->EnableKeyword(StringW(keyword));
        } else {
          material->DisableKeyword(StringW(keyword));
        }
        ApplyStereoKeywords(material);
        break;
      }
      case MaterialPropertyKind::Unsupported:
      default:
        break;
    }
  }
  void SaveOriginalGlobalProperty(MaterialPropertyChange const& property) {
    switch (property.kind) {
      case MaterialPropertyKind::Texture: {
        int propertyId = std::get<int>(property.id);
        if (!_savedGlobalProperties.contains(propertyId)) {
          _savedGlobalProperties.emplace(propertyId, UnityEngine::Shader::GetGlobalTexture(propertyId).ptr());
        }
        break;
      }
      case MaterialPropertyKind::Color: {
        int propertyId = std::get<int>(property.id);
        if (!_savedGlobalProperties.contains(propertyId)) {
          _savedGlobalProperties.emplace(propertyId, UnityEngine::Shader::GetGlobalColor(propertyId));
        }
        break;
      }
      case MaterialPropertyKind::Float: {
        int propertyId = std::get<int>(property.id);
        if (!_savedGlobalProperties.contains(propertyId)) {
          _savedGlobalProperties.emplace(propertyId, UnityEngine::Shader::GetGlobalFloat(propertyId));
        }
        break;
      }
      case MaterialPropertyKind::Vector: {
        int propertyId = std::get<int>(property.id);
        if (!_savedGlobalProperties.contains(propertyId)) {
          _savedGlobalProperties.emplace(propertyId, UnityEngine::Shader::GetGlobalVector(propertyId));
        }
        break;
      }
      case MaterialPropertyKind::Keyword: {
        auto const& keyword = std::get<std::string>(property.id);
        if (!_savedGlobalKeywords.contains(keyword)) {
          _savedGlobalKeywords.emplace(keyword, UnityEngine::Shader::IsKeywordEnabled(StringW(keyword)));
        }
        break;
      }
      case MaterialPropertyKind::Int:
      case MaterialPropertyKind::Unsupported:
      default:
        break;
    }
  }
  void ApplyGlobalProperty(MaterialPropertyChange const& property, float progress) {
    SaveOriginalGlobalProperty(property);
    switch (property.kind) {
      case MaterialPropertyKind::Texture: {
        int propertyId = std::get<int>(property.id);
        auto* texture = GetAssetAs<UnityEngine::Texture>(std::get<std::string>(property.value));
        if (texture != nullptr) {
          UnityEngine::Shader::SetGlobalTexture(propertyId, texture);
        }
        break;
      }
      case MaterialPropertyKind::Color: {
        int propertyId = std::get<int>(property.id);
        auto const& point = std::get<PointDefinitionW>(property.value);
        UnityEngine::Shader::SetGlobalColor(propertyId, VectorToColor(point.InterpolateVector4(progress)));
        break;
      }
      case MaterialPropertyKind::Float: {
        int propertyId = std::get<int>(property.id);
        if (std::holds_alternative<PointDefinitionW>(property.value)) {
          UnityEngine::Shader::SetGlobalFloat(propertyId, std::get<PointDefinitionW>(property.value).InterpolateLinear(progress));
        } else {
          UnityEngine::Shader::SetGlobalFloat(propertyId, std::get<float>(property.value));
        }
        break;
      }
      case MaterialPropertyKind::Vector: {
        int propertyId = std::get<int>(property.id);
        auto const& point = std::get<PointDefinitionW>(property.value);
        UnityEngine::Shader::SetGlobalVector(propertyId, ToUnityVector(point.InterpolateVector4(progress)));
        break;
      }
      case MaterialPropertyKind::Keyword: {
        auto const& keyword = std::get<std::string>(property.id);
        bool enabled = false;
        if (std::holds_alternative<PointDefinitionW>(property.value)) {
          enabled = std::get<PointDefinitionW>(property.value).InterpolateLinear(progress) >= 1.0f;
        } else {
          enabled = std::get<bool>(property.value);
        }
        if (enabled) {
          UnityEngine::Shader::EnableKeyword(StringW(keyword));
        } else {
          UnityEngine::Shader::DisableKeyword(StringW(keyword));
        }
        auto currentCamera = UnityEngine::Camera::get_current();
        SetMultipassShaderStateForCamera(currentCamera.unsafePtr());
        break;
      }
      case MaterialPropertyKind::Int:
      case MaterialPropertyKind::Unsupported:
      default:
        break;
    }
  }
  void ApplyAnimatorProperty(std::vector<UnityEngine::Animator*> const& animators,
                             AnimatorPropertyChange const& property,
                             float progress) {
    for (auto* animator : animators) {
      if (animator == nullptr) {
        continue;
      }
      switch (property.kind) {
        case AnimatorPropertyKind::Bool: {
          bool value = false;
          if (std::holds_alternative<PointDefinitionW>(property.value)) {
            value = std::get<PointDefinitionW>(property.value).InterpolateLinear(progress) >= 1.0f;
          } else {
            value = std::get<bool>(property.value);
          }
          animator->SetBool(StringW(property.name), value);
          break;
        }
        case AnimatorPropertyKind::Float: {
          float value = 0.0f;
          if (std::holds_alternative<PointDefinitionW>(property.value)) {
            value = std::get<PointDefinitionW>(property.value).InterpolateLinear(progress);
          } else {
            value = std::get<float>(property.value);
          }
          animator->SetFloat(StringW(property.name), value);
          break;
        }
        case AnimatorPropertyKind::Integer: {
          int value = 0;
          if (std::holds_alternative<PointDefinitionW>(property.value)) {
            value = static_cast<int>(std::get<PointDefinitionW>(property.value).InterpolateLinear(progress));
          } else {
            value = std::get<int>(property.value);
          }
          animator->SetInteger(StringW(property.name), value);
          break;
        }
        case AnimatorPropertyKind::Trigger: {
          bool enabled = std::get<bool>(property.value);
          if (enabled) {
            animator->SetTrigger(StringW(property.name));
          } else {
            animator->ResetTrigger(StringW(property.name));
          }
          break;
        }
        case AnimatorPropertyKind::Unsupported:
        default:
          break;
      }
    }
  }
  void HandleSetMaterialProperty(CustomJSONData::CustomEventData* customEventData, rapidjson::Value const& json) {
    auto asset = ReadStringView(json, "asset");
    if (!asset.has_value()) {
      return;
    }
    auto* material = GetAssetAs<UnityEngine::Material>(*asset);
    if (material == nullptr) {
      return;
    }
    auto properties = ParseMaterialProperties(json);
    if (properties.empty()) {
      return;
    }
    float duration = DurationBeatsToSeconds(ReadFloat(json, "duration").value_or(0.0f));
    Functions easing = ParseEasing(ReadStringView(json, "easing").value_or("easeLinear"sv));
    float startTime = customEventData->time;
    std::vector<MaterialPropertyChange> animatedProperties;
    animatedProperties.reserve(properties.size());
    float currentSongTime = CurrentSongTime();
    bool completed = duration <= 0.0f || startTime + duration <= currentSongTime;
    float initialProgress = completed ? 1.0f : 0.0f;
    for (auto const& property : properties) {
      ApplyMaterialProperty(material, property, initialProgress);
      if (!completed && IsAnimated(property)) {
        animatedProperties.emplace_back(property);
      }
    }
    if (!animatedProperties.empty()) {
      _materialAnimations.emplace_back(ActiveMaterialAnimation{
          .material = material,
          .properties = std::move(animatedProperties),
          .startTime = startTime,
          .duration = duration,
          .endTime = startTime + duration,
          .easing = easing,
      });
    }
  }
  void HandleSetAnimatorProperty(CustomJSONData::CustomEventData* customEventData, rapidjson::Value const& json) {
    auto id = ReadStringView(json, "id");
    if (!id.has_value()) {
      return;
    }
    std::string prefabId(*id);
    auto prefabIt = _livePrefabs.find(prefabId);
    if (prefabIt == _livePrefabs.end()) {
      return;
    }
    auto properties = ParseAnimatorProperties(json);
    if (properties.empty()) {
      return;
    }
    float duration = DurationBeatsToSeconds(ReadFloat(json, "duration").value_or(0.0f));
    Functions easing = ParseEasing(ReadStringView(json, "easing").value_or("easeLinear"sv));
    float startTime = customEventData->time;
    std::vector<AnimatorPropertyChange> animatedProperties;
    animatedProperties.reserve(properties.size());
    float currentSongTime = CurrentSongTime();
    bool completed = duration <= 0.0f || startTime + duration <= currentSongTime;
    float initialProgress = completed ? 1.0f : 0.0f;
    for (auto const& property : properties) {
      ApplyAnimatorProperty(prefabIt->second.animators, property, initialProgress);
      if (!completed && IsAnimated(property)) {
        animatedProperties.emplace_back(property);
      }
    }
    if (!animatedProperties.empty()) {
      _animatorAnimations.emplace_back(ActiveAnimatorAnimation{
          .prefabId = std::move(prefabId),
          .properties = std::move(animatedProperties),
          .startTime = startTime,
          .duration = duration,
          .endTime = startTime + duration,
          .easing = easing,
      });
    }
  }
  void HandleSetGlobalProperty(CustomJSONData::CustomEventData* customEventData, rapidjson::Value const& json) {
    auto properties = ParseMaterialProperties(json);
    if (properties.empty()) {
      return;
    }
    float duration = DurationBeatsToSeconds(ReadFloat(json, "duration").value_or(0.0f));
    Functions easing = ParseEasing(ReadStringView(json, "easing").value_or("easeLinear"sv));
    float startTime = customEventData->time;
    std::vector<MaterialPropertyChange> animatedProperties;
    animatedProperties.reserve(properties.size());
    float currentSongTime = CurrentSongTime();
    bool completed = duration <= 0.0f || startTime + duration <= currentSongTime;
    float initialProgress = completed ? 1.0f : 0.0f;
    for (auto const& property : properties) {
      ApplyGlobalProperty(property, initialProgress);
      if (!completed && IsAnimated(property)) {
        animatedProperties.emplace_back(property);
      }
    }
    if (!animatedProperties.empty()) {
      _globalAnimations.emplace_back(ActiveGlobalAnimation{
          .properties = std::move(animatedProperties),
          .startTime = startTime,
          .duration = duration,
          .endTime = startTime + duration,
          .easing = easing,
      });
    }
  }
  float AnimationProgress(float startTime, float duration, Functions easing, float songTime) const {
    if (duration <= 0.0f) {
      return 1.0f;
    }
    float raw = std::clamp((songTime - startTime) / duration, 0.0f, 1.0f);
    return Easings::Interpolate(raw, easing);
  }
  void UpdateMaterialAnimations() {
    if (_materialAnimations.empty()) return;
    float const songTime = CurrentSongTime();
    auto write = _materialAnimations.begin();
    for (auto read = _materialAnimations.begin(); read != _materialAnimations.end(); ++read) {
      if (read->material == nullptr) continue;
      float const progress = AnimationProgress(read->startTime, read->duration, read->easing, songTime);
      for (auto const& property : read->properties) ApplyMaterialProperty(read->material, property, progress);
      if (songTime < read->endTime) {
        if (write != read) *write = std::move(*read);
        ++write;
      }
    }
    _materialAnimations.erase(write, _materialAnimations.end());
  }
  void UpdateGlobalAnimations() {
    if (_globalAnimations.empty()) return;
    float const songTime = CurrentSongTime();
    auto write = _globalAnimations.begin();
    for (auto read = _globalAnimations.begin(); read != _globalAnimations.end(); ++read) {
      float const progress = AnimationProgress(read->startTime, read->duration, read->easing, songTime);
      for (auto const& property : read->properties) ApplyGlobalProperty(property, progress);
      if (songTime < read->endTime) {
        if (write != read) *write = std::move(*read);
        ++write;
      }
    }
    _globalAnimations.erase(write, _globalAnimations.end());
  }
  void UpdateAnimatorAnimations() {
    if (_animatorAnimations.empty()) return;
    float const songTime = CurrentSongTime();
    auto write = _animatorAnimations.begin();
    for (auto read = _animatorAnimations.begin(); read != _animatorAnimations.end(); ++read) {
      auto prefabIt = _livePrefabs.find(read->prefabId);
      if (prefabIt == _livePrefabs.end()) continue;
      float const progress = AnimationProgress(read->startTime, read->duration, read->easing, songTime);
      for (auto const& property : read->properties) ApplyAnimatorProperty(prefabIt->second.animators, property, progress);
      if (songTime < read->endTime) {
        if (write != read) *write = std::move(*read);
        ++write;
      }
    }
    _animatorAnimations.erase(write, _animatorAnimations.end());
  }
#include "VivifyNewHandlers.inl"
  void RestoreGlobalProperties() {
    for (auto const& [propertyId, value] : _savedGlobalProperties) {
      if (std::holds_alternative<UnityEngine::Texture*>(value)) {
        UnityEngine::Shader::SetGlobalTexture(propertyId, std::get<UnityEngine::Texture*>(value));
      } else if (std::holds_alternative<UnityEngine::Color>(value)) {
        UnityEngine::Shader::SetGlobalColor(propertyId, std::get<UnityEngine::Color>(value));
      } else if (std::holds_alternative<float>(value)) {
        UnityEngine::Shader::SetGlobalFloat(propertyId, std::get<float>(value));
      } else if (std::holds_alternative<UnityEngine::Vector4>(value)) {
        UnityEngine::Shader::SetGlobalVector(propertyId, std::get<UnityEngine::Vector4>(value));
      }
    }
    for (auto const& [keyword, enabled] : _savedGlobalKeywords) {
      if (enabled) {
        UnityEngine::Shader::EnableKeyword(StringW(keyword));
      } else {
        UnityEngine::Shader::DisableKeyword(StringW(keyword));
      }
    }
  }
  RuntimeBehaviour* _behaviour = nullptr;
  CustomJSONData::CustomBeatmapData* _currentBeatmapData = nullptr;
  GlobalNamespace::AudioTimeSyncController* _audioTimeSyncController = nullptr;
  TracksAD::BeatmapAssociatedData* _beatmapAD = nullptr;
  TracksAD::BeatmapAssociatedData _fallbackBeatmapAD;
  UnityEngine::AssetBundle* _mainBundle = nullptr;
  std::string _selectedLevelPath;
  std::string _preloadedBundlePath; // path of the bundle preloaded at level selection
  bool _selectedMapHasVivifyRequirement = false;
  std::unordered_map<std::string, UnityEngine::Object*> _assets;
  std::unordered_map<std::string, UnityEngine::Object*> _assetsByName;
  std::unordered_map<std::string, UnityEngine::Shader*> _supportedShadersByName;
  std::unordered_map<CustomJSONData::CustomEventData*, InstantiatePrefabData> _instantiatePrefabs;
  std::unordered_map<std::string, LivePrefab> _livePrefabs;
  std::vector<ActiveMaterialAnimation> _materialAnimations;
  std::vector<ActiveGlobalAnimation> _globalAnimations;
  std::vector<ActiveAnimatorAnimation> _animatorAnimations;
  // Snapshots of in-progress animations taken at pause time; restored on resume.
  std::vector<ActiveMaterialAnimation> _pausedMaterialAnimations;
  std::vector<ActiveGlobalAnimation> _pausedGlobalAnimations;
  std::vector<ActiveAnimatorAnimation> _pausedAnimatorAnimations;
  std::vector<ActiveRenderSettingAnimation> _pausedRenderSettingAnimations;
  std::unordered_map<int, SavedGlobalValue> _savedGlobalProperties;
  std::unordered_map<std::string, bool> _savedGlobalKeywords;
  std::unordered_set<std::string> _unsupportedEventWarnings;
  std::unordered_set<UnityEngine::Material*> _repairedMaterials;
  std::unordered_map<UnityEngine::Renderer*, int> _overlayRendererSortingOrders;
  std::unordered_map<std::string, DeclaredTextureData> _declaredTextures;
  std::unordered_map<std::string, SecondaryCameraData> _secondaryCameras;
  std::unordered_map<std::string, CameraPropertyData> _cameraProperties;
  std::vector<std::pair<TrackW, int>> _cameraCullingLayers;
  std::vector<ActiveBlitEffect> _preEffects;
  std::vector<ActiveBlitEffect> _postEffects;
  std::vector<ActiveRenderSettingAnimation> _renderSettingAnimations;
  std::vector<SavedRenderSetting> _savedRenderSettings;
  std::vector<AssignedPrefabInfo> _assignedPrefabs;
  std::vector<std::vector<AssignedPrefabInfo*>> _activeDebrisPrefabStack;
  std::vector<AssignedPrefabInfo*> _lastCutDebrisPrefabs;
  std::vector<ActiveSaberVisual> _activeSabers;
  std::unordered_map<GlobalNamespace::NoteController*, VisualReplacement> _noteReplacements;
  std::unordered_map<GlobalNamespace::SaberModelController*, VisualReplacement> _saberReplacements;
  std::unordered_map<GlobalNamespace::NoteDebris*, VisualReplacement> _debrisReplacements;
  CameraApplier* _cameraApplier = nullptr;
  UnityEngine::Camera* _gameplayOverlayCamera = nullptr;
  int _gameplayOverlayLayerMask = 0;
  MultipassKeywordController* _multipassController = nullptr;
  UnityEngine::RenderTexture* _mainBlitTexture = nullptr;
  UnityEngine::RenderTexture* _scratchBlitTexture = nullptr;
  std::vector<UnityEngine::Video::VideoPlayer*> _videoPlayers;
  std::unordered_map<std::string, std::string> _assetPaths;
  std::unordered_set<CustomJSONData::CustomEventData*> _catchUpAppliedCustomEvents;
  float _lastSongTime = -1.0f;
  int _songTimeCacheFrame = -1;
  float _songTimeCache = 0.0f;
  bool _loggedUnityPlatformInfo = false;
  bool _pauseMenuActive = false;
  bool _isResetting = false;
};
}
MAKE_HOOK_MATCH(SaberModelController_Init, &GlobalNamespace::SaberModelController::Init, void, GlobalNamespace::SaberModelController* self, UnityEngine::Transform* parent, GlobalNamespace::Saber* saber, UnityEngine::Color trailTintColor) {
  SaberModelController_Init(self, parent, saber, trailTintColor);
  Runtime::Instance().TrackSaberModel(self, saber, parent);
}
MAKE_HOOK_MATCH(GameNoteController_Init, &GlobalNamespace::GameNoteController::Init, void, GlobalNamespace::GameNoteController* self, GlobalNamespace::NoteData* noteData, ByRef<GlobalNamespace::NoteSpawnData> noteSpawnData, GlobalNamespace::NoteVisualModifierType noteVisualModifierType, float cutAngleTolerance, float uniformScale) {
  GameNoteController_Init(self, noteData, noteSpawnData, noteVisualModifierType, cutAngleTolerance, uniformScale);
  Runtime::Instance().RestoreNoteVisuals(self);
  if (Runtime::Instance().GetCurrentBeatmapData() == nullptr || Runtime::Instance().IsResetting()) return;
  if (noteData == nullptr || noteData->get_gameplayType() != GlobalNamespace::NoteData_GameplayType::Normal) return;
  Runtime::Instance().ApplyTrackCameraCullingToNote(self, noteData);
  AssignedPrefabKind kind = noteData->get_cutDirection().value__ == GlobalNamespace::NoteCutDirection::Any.value__
      ? AssignedPrefabKind::AnyDirectionObject
      : AssignedPrefabKind::Object;
  auto infos = Runtime::Instance().FindAssignedPrefabs("colorNotes", noteData, kind);
  if (!infos.empty() && !GetDisableCustomNoteVisuals()) {
    auto* gameObject = SafeUnityPtr(self->get_gameObject());
    if (IsManagedAlive(gameObject)) {
      Runtime::Instance().ForceGameObjectRenderersOnTop(gameObject);
    }
    Runtime::Instance().ReplaceNoteVisuals(self, infos);
  }
}
MAKE_HOOK_MATCH(BombNoteController_Init, &GlobalNamespace::BombNoteController::Init, void, GlobalNamespace::BombNoteController* self, GlobalNamespace::NoteData* noteData, ByRef<GlobalNamespace::NoteSpawnData> noteSpawnData) {
  BombNoteController_Init(self, noteData, noteSpawnData);
  Runtime::Instance().RestoreNoteVisuals(self);
  if (Runtime::Instance().GetCurrentBeatmapData() == nullptr || Runtime::Instance().IsResetting()) return;
  Runtime::Instance().ApplyTrackCameraCullingToNote(self, noteData);
  auto infos = Runtime::Instance().FindAssignedPrefabs("bombNotes", noteData, AssignedPrefabKind::Object);
  if (!infos.empty() && !GetDisableCustomNoteVisuals()) {
    auto* gameObject = SafeUnityPtr(self->get_gameObject());
    if (IsManagedAlive(gameObject)) {
      Runtime::Instance().ForceGameObjectRenderersOnTop(gameObject);
    }
    Runtime::Instance().ReplaceNoteVisuals(self, infos);
  }
}
MAKE_HOOK_MATCH(BurstSliderGameNoteController_Init, &GlobalNamespace::BurstSliderGameNoteController::Init, void, GlobalNamespace::BurstSliderGameNoteController* self, GlobalNamespace::NoteData* noteData, ByRef<GlobalNamespace::NoteSpawnData> noteSpawnData, GlobalNamespace::NoteVisualModifierType noteVisualModifierType, float uniformScale) {
  BurstSliderGameNoteController_Init(self, noteData, noteSpawnData, noteVisualModifierType, uniformScale);
  Runtime::Instance().RestoreNoteVisuals(self);
  if (Runtime::Instance().GetCurrentBeatmapData() == nullptr || Runtime::Instance().IsResetting()) return;
  if (noteData == nullptr) return;
  Runtime::Instance().ApplyTrackCameraCullingToNote(self, noteData);
  auto* objectType = noteData->get_gameplayType() == GlobalNamespace::NoteData_GameplayType::BurstSliderHead ? "burstSliders" : "burstSliderElements";
  auto infos = Runtime::Instance().FindAssignedPrefabs(objectType, noteData, AssignedPrefabKind::Object);
  if (!infos.empty() && !GetDisableCustomNoteVisuals()) {
    auto* gameObject = SafeUnityPtr(self->get_gameObject());
    if (IsManagedAlive(gameObject)) {
      Runtime::Instance().ForceGameObjectRenderersOnTop(gameObject);
    }
    Runtime::Instance().ReplaceNoteVisuals(self, infos);
  }
}
MAKE_HOOK_MATCH(NoteCutCoreEffectsSpawner_SpawnNoteCutEffect, &GlobalNamespace::NoteCutCoreEffectsSpawner::SpawnNoteCutEffect, void, GlobalNamespace::NoteCutCoreEffectsSpawner* self, ByRef<GlobalNamespace::NoteCutInfo> noteCutInfo, GlobalNamespace::NoteController* noteController, int32_t sparkleParticlesCount, int32_t explosionParticlesCount) {
  if (Runtime::Instance().GetCurrentBeatmapData() == nullptr || Runtime::Instance().IsResetting()) {
    NoteCutCoreEffectsSpawner_SpawnNoteCutEffect(self, noteCutInfo, noteController, sparkleParticlesCount, explosionParticlesCount);
    return;
  }
  // Only push custom debris prefabs when the base spawner is actually producing particle
  // effects. If the engine would not spawn debris (zero particles), avoid applying
  // replacements to prevent custom debris appearing when no debris is expected.
  if (sparkleParticlesCount > 0 || explosionParticlesCount > 0) {
    Runtime::Instance().PushActiveDebrisPrefabs(Runtime::Instance().FindAssignedDebrisPrefabs(noteCutInfo->noteData));
  } else {
    // Ensure stack state remains balanced by pushing an empty vector so PopActiveDebrisPrefabs
    // still clears the last-cut prefabs correctly.
    Runtime::Instance().PushActiveDebrisPrefabs({});
  }
  NoteCutCoreEffectsSpawner_SpawnNoteCutEffect(self, noteCutInfo, noteController, sparkleParticlesCount, explosionParticlesCount);
  Runtime::Instance().PopActiveDebrisPrefabs();
}
MAKE_HOOK_MATCH(NoteDebris_Init, &GlobalNamespace::NoteDebris::Init, void, GlobalNamespace::NoteDebris* self, GlobalNamespace::ColorType colorType, UnityEngine::Vector3 notePos, UnityEngine::Quaternion noteRot, UnityEngine::Vector3 noteMoveVec, UnityEngine::Vector3 noteScale, UnityEngine::Vector3 positionOffset, UnityEngine::Quaternion rotationOffset, UnityEngine::Vector3 cutPoint, UnityEngine::Vector3 cutNormal, UnityEngine::Vector3 force, UnityEngine::Vector3 torque, float lifeTime, UnityEngine::Vector3 cutoutOffset, bool forceOnlySimplePhysics) {
  NoteDebris_Init(self, colorType, notePos, noteRot, noteMoveVec, noteScale, positionOffset, rotationOffset, cutPoint, cutNormal, force, torque, lifeTime, cutoutOffset, forceOnlySimplePhysics);
  Runtime::Instance().RestoreDebrisVisuals(self);
  if (Runtime::Instance().GetCurrentBeatmapData() == nullptr || Runtime::Instance().IsResetting()) return;
  if (!GetDisableCustomNoteVisuals()) {
    Runtime::Instance().ReplaceDebrisVisuals(self);
  }
}
MAKE_HOOK_MATCH(AlwaysVisibleQuad_OnEnable, &GlobalNamespace::AlwaysVisibleQuad::OnEnable, void, GlobalNamespace::AlwaysVisibleQuad* self) {
  AlwaysVisibleQuad_OnEnable(self);
  if (Runtime::Instance().GetCurrentBeatmapData() == nullptr || Runtime::Instance().IsResetting()) return;
  if (!IsManagedAlive(self)) return;
  auto transform = self->get_transform();
  auto* rawTransform = IsManagedAlive(transform) ? transform.unsafePtr() : nullptr;
  if (rawTransform == nullptr) return;
  rawTransform->set_position(UnityEngine::Vector3(0.0f, -1000.0f, 0.0f));
}
bool ShouldSkipVRCenterAdjust(GlobalNamespace::VRCenterAdjust* self, std::string_view method) {
  if (!IsManagedAlive(self)) return true;
  bool const missingSettingsManager = self->____settingsManager == nullptr;
  bool const missingSettingsApplicator = !IsManagedAlive(self->____settingsApplicator.unsafePtr());
  if (GetDisableVRCenterAdjust()) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("VRCenterAdjust.{} skipped: isolation toggle enabled", method);
    }
    return true;
  }
  if (missingSettingsManager || missingSettingsApplicator) {
    if (method != "Update"sv || GetVivifyDebugLogging()) {
      PaperLogger.warn("VRCenterAdjust.{} skipped: missing _settingsManager={} _settingsApplicator={}",
                       method, BoolText(missingSettingsManager), BoolText(missingSettingsApplicator));
    }
    return true;
  }
  return false;
}
MAKE_HOOK_MATCH(VRCenterAdjust_Start, &GlobalNamespace::VRCenterAdjust::Start, void, GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipVRCenterAdjust(self, "Start")) return;
  VRCenterAdjust_Start(self);
}
MAKE_HOOK_MATCH(VRCenterAdjust_OnEnable, &GlobalNamespace::VRCenterAdjust::OnEnable, void, GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipVRCenterAdjust(self, "OnEnable")) return;
  VRCenterAdjust_OnEnable(self);
}
MAKE_HOOK_MATCH(VRCenterAdjust_OnDisable, &GlobalNamespace::VRCenterAdjust::OnDisable, void, GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipVRCenterAdjust(self, "OnDisable")) return;
  VRCenterAdjust_OnDisable(self);
}
MAKE_HOOK_MATCH(VRCenterAdjust_Update, &GlobalNamespace::VRCenterAdjust::Update, void, GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipVRCenterAdjust(self, "Update")) return;
  VRCenterAdjust_Update(self);
}
MAKE_HOOK_MATCH(VRCenterAdjust_ResetRoom, &GlobalNamespace::VRCenterAdjust::ResetRoom, void, GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipVRCenterAdjust(self, "ResetRoom")) return;
  VRCenterAdjust_ResetRoom(self);
}
MAKE_HOOK_MATCH(VRCenterAdjust_SetRoomTransformOffset, &GlobalNamespace::VRCenterAdjust::SetRoomTransformOffset, void, GlobalNamespace::VRCenterAdjust* self) {
  if (ShouldSkipVRCenterAdjust(self, "SetRoomTransformOffset")) return;
  VRCenterAdjust_SetRoomTransformOffset(self);
}
MAKE_HOOK_MATCH(PauseController_Pause, &GlobalNamespace::PauseController::Pause, void, GlobalNamespace::PauseController* self) {
  Runtime::Instance().SetPauseMenuActive(true);
  PauseController_Pause(self);
}
MAKE_HOOK_MATCH(PauseMenuManager_ShowMenu, &GlobalNamespace::PauseMenuManager::ShowMenu, void, GlobalNamespace::PauseMenuManager* self) {
  Runtime::Instance().SetPauseMenuActive(true);
  PauseMenuManager_ShowMenu(self);
}
MAKE_HOOK_MATCH(PauseMenuManager_MenuButtonPressed, &GlobalNamespace::PauseMenuManager::MenuButtonPressed, void, GlobalNamespace::PauseMenuManager* self) {
  Runtime::Instance().SetPauseMenuActive(true);
  PauseMenuManager_MenuButtonPressed(self);
}
MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidPressMenuButton, &GlobalNamespace::PauseController::HandlePauseMenuManagerDidPressMenuButton, void, GlobalNamespace::PauseController* self) {
  Runtime::Instance().SetPauseMenuActive(true);
  PauseController_HandlePauseMenuManagerDidPressMenuButton(self);
}
MAKE_HOOK_MATCH(PauseController_HandlePauseMenuManagerDidFinishResumeAnimation, &GlobalNamespace::PauseController::HandlePauseMenuManagerDidFinishResumeAnimation, void, GlobalNamespace::PauseController* self) {
  PauseController_HandlePauseMenuManagerDidFinishResumeAnimation(self);
  Runtime::Instance().SetPauseMenuActive(false);
}
MAKE_HOOK_MATCH(GamePause_Resume, &GlobalNamespace::GamePause::Resume, void, GlobalNamespace::GamePause* self) {
  GamePause_Resume(self);
  Runtime::Instance().SetPauseMenuActive(false);
}
void LateLoad() {
  Runtime::Instance().LateLoad();
  INSTALL_HOOK(PaperLogger, SaberModelController_Init);
  INSTALL_HOOK(PaperLogger, GameNoteController_Init);
  INSTALL_HOOK(PaperLogger, BombNoteController_Init);
  INSTALL_HOOK(PaperLogger, BurstSliderGameNoteController_Init);
  INSTALL_HOOK(PaperLogger, NoteCutCoreEffectsSpawner_SpawnNoteCutEffect);
  INSTALL_HOOK(PaperLogger, NoteDebris_Init);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_Start);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_OnEnable);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_OnDisable);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_Update);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_ResetRoom);
  INSTALL_HOOK(PaperLogger, VRCenterAdjust_SetRoomTransformOffset);
  INSTALL_HOOK(PaperLogger, PauseController_Pause);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_ShowMenu);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_MenuButtonPressed);
  INSTALL_HOOK(PaperLogger, PauseController_HandlePauseMenuManagerDidPressMenuButton);
  INSTALL_HOOK(PaperLogger, PauseController_HandlePauseMenuManagerDidFinishResumeAnimation);
  INSTALL_HOOK(PaperLogger, GamePause_Resume);
}
void RefreshMultipassRendering() {
  Runtime::Instance().RefreshMultipassRendering();
}
void RefreshIsolationSettings() {
  Runtime::Instance().RefreshIsolationSettings();
}
void RuntimeBehaviour::Update() {
  Runtime::Instance().Update();
}
void RuntimeBehaviour::OnDestroy() {
  Runtime::Instance().OnBehaviourDestroyed(this);
}
void CameraApplier::OnRenderImage(UnityEngine::RenderTexture* src, UnityEngine::RenderTexture* dest) {
  Runtime::Instance().ApplyBlits(src, dest);
}
}

bool SameBlitData(BlitMaterialData const& left, BlitMaterialData const& right) {
  return left.material == right.material &&
         left.priority == right.priority &&
         left.source == right.source &&
         left.targets == right.targets &&
         left.pass == right.pass;
}
std::optional<std::string_view> ReadBlitAssetName(rapidjson::Value const& json) {
  if (auto asset = ReadStringView(json, "asset")) return asset;
  if (auto material = ReadStringView(json, "material")) return material;
  if (auto effect = ReadStringView(json, "effect")) return effect;
  if (auto postMaterial = ReadStringView(json, "postProcessMaterial")) return postMaterial;
  if (auto postMaterial = ReadStringView(json, "postProcessingMaterial")) return postMaterial;
  return std::nullopt;
}
PostProcessingOrder ParsePostProcessingOrder(rapidjson::Value const& json) {
  if (auto beforeMain = ReadBool(json, "beforeMainEffect"); beforeMain.has_value()) {
    return beforeMain.value() ? PostProcessingOrder::BeforeMainEffect : PostProcessingOrder::AfterMainEffect;
  }
  if (auto afterMain = ReadBool(json, "afterMainEffect"); afterMain.has_value()) {
    return afterMain.value() ? PostProcessingOrder::AfterMainEffect : PostProcessingOrder::BeforeMainEffect;
  }
  auto order = ReadStringView(json, "order");
  if (!order.has_value()) order = ReadStringView(json, "phase");
  if (!order.has_value()) order = ReadStringView(json, "timing");
  if (!order.has_value()) return PostProcessingOrder::AfterMainEffect;
  std::string normalized = NormalizeAssetKey(*order);
  if (normalized == "beforemaineffect" || normalized == "beforemain" ||
      normalized == "before" || normalized == "pre") {
    return PostProcessingOrder::BeforeMainEffect;
  }
  return PostProcessingOrder::AfterMainEffect;
}
void RemoveMatchingBlitEffects(std::vector<ActiveBlitEffect>& effects,
                               BlitMaterialData const& filter,
                               bool filterAsset,
                               bool filterSource,
                               bool filterTargets,
                               bool filterPriority,
                               bool filterPass) {
  size_t const before = effects.size();
  effects.erase(std::remove_if(effects.begin(), effects.end(), [&](ActiveBlitEffect const& active) {
    if (filterAsset && active.data.asset != filter.asset) return false;
    if (filterSource && active.data.source != filter.source) return false;
    if (filterTargets && active.data.targets != filter.targets) return false;
    if (filterPriority && active.data.priority != filter.priority) return false;
    if (filterPass && active.data.pass != filter.pass) return false;
    return true;
  }), effects.end());
}
void AddOrUpdateBlitEffect(std::vector<ActiveBlitEffect>& effects, ActiveBlitEffect effect) {
  auto existing = std::find_if(effects.begin(), effects.end(), [&](ActiveBlitEffect const& active) {
    return SameBlitData(active.data, effect.data);
  });
  // Always mark dirty — either a new effect is being added or an existing one updated.
  if (existing != effects.end()) {
    if (effect.data.frame.has_value()) {
      existing->data.frame = effect.data.frame;
      existing->expireTime = 0.0f;
    } else {
      existing->data.frame.reset();
      existing->expireTime = effect.expireTime <= 0.0f ? 0.0f : std::max(existing->expireTime, effect.expireTime);
    }
  } else {
    effects.emplace_back(std::move(effect));
  }
  std::stable_sort(effects.begin(), effects.end(), [](ActiveBlitEffect const& left, ActiveBlitEffect const& right) {
    return left.data.priority < right.data.priority;
  });
  while (effects.size() > kMaxActiveBlitEffects) {
    effects.erase(effects.begin());
  }
}
void HandleBlit(CustomJSONData::CustomEventData* customEventData, rapidjson::Value const& json) {
  auto asset = ReadBlitAssetName(json);
  std::string normalizedAsset = asset.has_value() ? NormalizeAssetKey(*asset) : std::string();
  if (GetDisableAllBlits()) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify {} skipped at beat {}: Disable All Blits enabled asset='{}'",
                       "Blit",
                       customEventData->time,
                       asset.has_value() ? std::string(*asset) : std::string("<none>"));
    }
    return;
  }
  if (asset.has_value()) {
    if (GetDisableBeat0FilmgrainBlit() && std::fabs(customEventData->time) < 0.01f &&
        normalizedAsset.find("filmgrain") != std::string::npos) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify {} skipped at beat {}: beat-0 filmgrain isolation asset='{}'",
                         "Blit", customEventData->time, std::string(*asset));
      }
      return;
    }
  }
  bool clear = ReadBool(json, "clear").value_or(false) || ReadBool(json, "remove").value_or(false) ||
               ReadBool(json, "enabled").value_or(true) == false;
  UnityEngine::Material* material = nullptr;
  if (asset.has_value() && !clear) {
    material = GetAssetAs<UnityEngine::Material>(*asset);
    if (material == nullptr) {
      if (GetVivifyDebugLogging()) {
        PaperLogger.warn("Vivify {} material load failed at beat {}: asset='{}'",
                         "Blit", customEventData->time, std::string(*asset));
      }
      return;
    }
    RepairMaterialShader(material, *asset);
    LogMaterialShader("Blit", *asset, material);
    auto properties = ParseMaterialProperties(json);
    if (!properties.empty()) {
      float dur = DurationBeatsToSeconds(ReadFloat(json, "duration").value_or(0.0f));
      Functions eas = ParseEasing(ReadStringView(json, "easing").value_or("easeLinear"));
      float st = customEventData->time;
      float cur = CurrentSongTime();
      bool done = dur <= 0.0f || st + dur <= cur;
      float prog = done ? 1.0f : 0.0f;
      for (auto const& p : properties) ApplyMaterialProperty(material, p, prog);
      if (!done) {
        std::vector<MaterialPropertyChange> animated;
        for (auto const& p : properties) { if (IsAnimated(p)) animated.emplace_back(p); }
        if (!animated.empty()) {
          _materialAnimations.emplace_back(ActiveMaterialAnimation{
            .material = material, .properties = std::move(animated),
            .startTime = st, .duration = dur, .endTime = st + dur, .easing = eas});
        }
      }
    }
  }
  int priority = ReadInt(json, "priority").value_or(0);
  int pass = ReadInt(json, "pass").value_or(-1);
  auto source = ReadStringView(json, "source");
  std::string sourceStr = source.has_value() ? std::string(*source) : "_Main";
  std::vector<std::string> targets;
  auto destVal = ReadValuePtr(json, "destination");
  if (destVal != nullptr) {
    if (destVal->IsString()) targets.emplace_back(destVal->GetString());
    else if (destVal->IsArray()) {
      for (auto const& e : destVal->GetArray()) {
        if (e.IsString()) targets.emplace_back(e.GetString());
      }
    }
  }
  if (targets.empty()) targets.emplace_back("_Main");
  bool pre = ParsePostProcessingOrder(json) == PostProcessingOrder::BeforeMainEffect;
  auto& effects = pre ? _preEffects : _postEffects;
  float durationBeats = ReadFloat(json, "duration").value_or(0.0f);
  float duration = DurationBeatsToSeconds(durationBeats);
  BlitMaterialData bd{material, priority, sourceStr, targets, pass, std::nullopt, normalizedAsset, customEventData->time};
  if (clear) {
    bool const filterAsset = asset.has_value();
    bool const filterSource = ReadValuePtr(json, "source") != nullptr;
    bool const filterTargets = destVal != nullptr;
    bool const filterPriority = ReadValuePtr(json, "priority") != nullptr;
    bool const filterPass = ReadValuePtr(json, "pass") != nullptr;
    RemoveMatchingBlitEffects(effects, bd, filterAsset, filterSource, filterTargets, filterPriority, filterPass);
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify {} cleared: beat={} asset='{}' source='{}' targets={} order={}",
                       "Blit",
                       customEventData->time,
                       asset.has_value() ? std::string(*asset) : std::string("<any>"),
                       filterSource ? sourceStr : std::string("<any>"),
                       filterTargets ? targets.size() : 0,
                       pre ? "BeforeMainEffect" : "AfterMainEffect");
    }
    return;
  }
  if (GetVivifyDebugLogging()) {
    std::string targetLog;
    for (auto const& target : targets) {
      if (!targetLog.empty()) targetLog += ",";
      targetLog += target;
    }
    auto* shader = material != nullptr ? material->get_shader().unsafePtr() : nullptr;
    PaperLogger.info(
        "Vivify {} event: beat={} asset='{}' shader='{}' source='{}' targets='{}' priority={} pass={} order={} durationBeats={} persistent={} xrOcclusionMesh={}",
        "Blit",
        customEventData->time,
        asset.has_value() ? std::string(*asset) : std::string("<none>"),
        ShaderNameForLog(shader),
        sourceStr,
        targetLog,
        priority,
        pass,
        pre ? "BeforeMainEffect" : "AfterMainEffect",
        durationBeats,
        BoolText(durationBeats < 0.0f),
        BoolText(UnityEngine::XR::XRSettings::get_useOcclusionMesh()));
  }
  float songTime = CurrentSongTime();
  if (durationBeats < 0.0f) {
    AddOrUpdateBlitEffect(effects, ActiveBlitEffect{bd, 0.0f});
  } else if (duration == 0) {
    bd.frame = static_cast<int>(UnityEngine::Time::get_frameCount());
    AddOrUpdateBlitEffect(effects, ActiveBlitEffect{bd, 0.0f});
  } else if (duration > 0 && songTime <= customEventData->time + duration) {
    AddOrUpdateBlitEffect(effects, ActiveBlitEffect{bd, customEventData->time + duration});
  }
}
void UpdateBlitEffects() {
  if (GetDisableAllBlits()) {
    if ((!_preEffects.empty() || !_postEffects.empty()) && GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify Blit effects cleared: Disable All Blits enabled pre={} post={}",
                       _preEffects.size(), _postEffects.size());
    }
    _preEffects.clear();
    _postEffects.clear();
    // CameraApplier enabled state is refreshed each Update() via RefreshCameraApplier.
    return;
  }
  float songTime = CurrentSongTime();
  int frame = static_cast<int>(UnityEngine::Time::get_frameCount());
  auto cleanup = [&](std::vector<ActiveBlitEffect>& effects) {
    size_t const before = effects.size();
    effects.erase(std::remove_if(effects.begin(), effects.end(), [&](ActiveBlitEffect const& e) {
      if (GetDisableBeat0FilmgrainBlit() && std::fabs(e.data.eventBeat) < 0.01f &&
          e.data.asset.find("filmgrain") != std::string::npos) {
        if (GetVivifyDebugLogging()) {
          PaperLogger.info("Vivify Blit effect removed: beat-0 filmgrain isolation asset='{}'", e.data.asset);
        }
        return true;
      }
      if (e.data.frame.has_value()) return e.data.frame.value() != frame;
      return e.expireTime > 0.0f && songTime >= e.expireTime;
    }), effects.end());
  };
  cleanup(_preEffects);
  cleanup(_postEffects);
}
void HandleCreateScreenTexture(rapidjson::Value const& json) {
  auto id = ReadStringView(json, "id");
  if (!id.has_value()) return;
  std::string name(*id);
  if (auto existing = _declaredTextures.find(name); existing != _declaredTextures.end()) {
    ReleaseDeclaredTextureData(existing->second);
    _declaredTextures.erase(existing);
  }
  DeclaredTextureData dt;
  dt.name = name;
  dt.propertyId = UnityEngine::Shader::PropertyToID(StringW(name));
  dt.xRatio = ReadFloat(json, "xRatio").value_or(1.0f);
  dt.yRatio = ReadFloat(json, "yRatio").value_or(1.0f);
  if (auto w = ReadInt(json, "width")) dt.width = *w;
  if (auto h = ReadInt(json, "height")) dt.height = *h;
  if (auto fmt = ReadStringView(json, "colorFormat"))
    dt.format = [&]() -> std::optional<UnityEngine::RenderTextureFormat> {
      auto s = *fmt;
      if (s == "ARGB32") return UnityEngine::RenderTextureFormat::ARGB32;
      if (s == "Depth") return UnityEngine::RenderTextureFormat::Depth;
      if (s == "ARGBHalf") return UnityEngine::RenderTextureFormat::ARGBHalf;
      if (s == "Shadowmap") return UnityEngine::RenderTextureFormat::Shadowmap;
      if (s == "RGB565") return UnityEngine::RenderTextureFormat::RGB565;
      if (s == "ARGB4444") return UnityEngine::RenderTextureFormat::ARGB4444;
      if (s == "ARGB1555") return UnityEngine::RenderTextureFormat::ARGB1555;
      if (s == "Default") return UnityEngine::RenderTextureFormat::Default;
      if (s == "ARGBFloat") return UnityEngine::RenderTextureFormat::ARGBFloat;
      if (s == "RGFloat") return UnityEngine::RenderTextureFormat::RGFloat;
      if (s == "RFloat") return UnityEngine::RenderTextureFormat::RFloat;
      if (s == "RHalf") return UnityEngine::RenderTextureFormat::RHalf;
      if (s == "R8") return UnityEngine::RenderTextureFormat::R8;
      if (s == "DefaultHDR") return UnityEngine::RenderTextureFormat::DefaultHDR;
      return std::nullopt;
    }();
  if (auto fm = ReadStringView(json, "filterMode"))
    dt.filterMode = [&]() -> std::optional<UnityEngine::FilterMode> {
      auto s = *fm;
      if (s == "Point") return UnityEngine::FilterMode::Point;
      if (s == "Bilinear") return UnityEngine::FilterMode::Bilinear;
      if (s == "Trilinear") return UnityEngine::FilterMode::Trilinear;
      return std::nullopt;
    }();
  if (dt.xRatio <= 0.0f) dt.xRatio = 1.0f;
  if (dt.yRatio <= 0.0f) dt.yRatio = 1.0f;
  int baseW = 1024;
  int baseH = 512;
  auto mainCam = UnityEngine::Camera::get_main();
  if (IsAlive(mainCam)) {
    baseW = std::max(1, mainCam->get_pixelWidth());
    baseH = std::max(1, mainCam->get_pixelHeight());
  }
  int w = std::clamp(dt.width.value_or(baseW), 1, kMaxRenderTextureSize);
  int h = std::clamp(dt.height.value_or(baseH), 1, kMaxRenderTextureSize);
  w = static_cast<int>(w / dt.xRatio);
  h = static_cast<int>(h / dt.yRatio);
  w = std::clamp(w, 1, kMaxRenderTextureSize);
  h = std::clamp(h, 1, kMaxRenderTextureSize);
  auto requestedFmt = dt.format.value_or(UnityEngine::RenderTextureFormat::ARGB32);
  auto fmt = SupportedRenderTextureFormat(requestedFmt, "CreateScreenTexture:" + name);
  if (GetVivifyDebugLogging()) {
    PaperLogger.info(
        "Vivify CreateScreenTexture: id='{}' size={}x{} requestedFormat={} finalFormat={} filter={} supportsRequested={} propertyId={} xrOcclusionMesh={}",
        name,
        w,
        h,
        requestedFmt.value__,
        fmt.value__,
        dt.filterMode.has_value() ? dt.filterMode->value__ : -1,
        BoolText(UnityEngine::SystemInfo::SupportsRenderTextureFormat(requestedFmt)),
        dt.propertyId,
        BoolText(UnityEngine::XR::XRSettings::get_useOcclusionMesh()));
  }
  dt.texture = UnityEngine::RenderTexture::New_ctor(w, h, 0, fmt);
  if (dt.filterMode.has_value()) dt.texture->set_filterMode(dt.filterMode.value());
  if (!IsAlive(dt.texture) || !dt.texture->Create()) {
    if (GetVivifyDebugLogging()) {
      PaperLogger.warn("Vivify CreateScreenTexture failed: id='{}' size={}x{} format={}", name, w, h, fmt.value__);
    }
    ReleaseDeclaredTextureData(dt);
    return;
  }
  UnityEngine::Shader::SetGlobalTexture(dt.propertyId, static_cast<UnityEngine::Texture*>(dt.texture));
  _declaredTextures[name] = std::move(dt);
}
void HandleCreateCamera(rapidjson::Value const& json) {
  auto id = ReadStringView(json, "id");
  if (!id.has_value()) return;
  std::string name(*id);
  if (GetDisableCreateCameraDepth()) {
    if (auto existing = _secondaryCameras.find(name); existing != _secondaryCameras.end()) {
      ReleaseSecondaryCameraData(existing->second);
      _secondaryCameras.erase(existing);
    }
    if (GetVivifyDebugLogging()) {
      auto texture = ReadStringView(json, "texture");
      auto depthTexture = ReadStringView(json, "depthTexture");
      PaperLogger.info("Vivify CreateCamera skipped: isolation toggle enabled id='{}' texture='{}' depthTexture='{}'",
                       name,
                       texture.has_value() ? std::string(*texture) : std::string("<none>"),
                       depthTexture.has_value() ? std::string(*depthTexture) : std::string("<none>"));
    }
    return;
  }
  if (auto existing = _secondaryCameras.find(name); existing != _secondaryCameras.end()) {
    ReleaseSecondaryCameraData(existing->second);
    _secondaryCameras.erase(existing);
  }
  SecondaryCameraData cam;
  cam.name = name;
  if (auto tex = ReadStringView(json, "texture")) {
    cam.textureName = std::string(*tex);
    cam.texturePropertyId = UnityEngine::Shader::PropertyToID(StringW(*tex));
  }
  if (auto dtex = ReadStringView(json, "depthTexture")) {
    if (!GetDisableCreateCameraDepth()) {
      cam.depthTextureName = std::string(*dtex);
      cam.depthTexturePropertyId = UnityEngine::Shader::PropertyToID(StringW(*dtex));
    } else if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify CreateCamera depth texture disabled: camera='{}' depthTexture='{}'", name, std::string(*dtex));
    }
  }
  auto* propsVal = ReadValuePtr(json, "properties");
  if (propsVal != nullptr && propsVal->IsObject()) {
    ParseCameraPropertyData(*propsVal, cam.properties);
  }
  if (GetDisableCreateCameraDepth()) {
    cam.properties.depthTextureMode.reset();
  }
  auto mainCam = UnityEngine::Camera::get_main();
  auto* go = UnityEngine::GameObject::New_ctor(StringW("VivifyCamera_" + name));
  if (mainCam != nullptr) {
    go->get_transform()->SetParent(mainCam->get_transform(), false);
  }
  cam.camera = go->AddComponent<UnityEngine::Camera*>();
  if (mainCam != nullptr) cam.camera->CopyFrom(mainCam);
  cam.camera->set_depth(mainCam ? mainCam->get_depth() - 1 : -2);
  EnsureMultipassKeywordController(go);
  int w = 1024, h = 512;
  if (mainCam != nullptr) { w = mainCam->get_pixelWidth(); h = mainCam->get_pixelHeight(); }
  if (cam.texturePropertyId.has_value()) {
    auto colorFormat = SupportedRenderTextureFormat(UnityEngine::RenderTextureFormat::ARGB32, "CreateCamera:color:" + name);
    cam.colorRT = UnityEngine::RenderTexture::New_ctor(w, h, 0, colorFormat);
    bool colorCreated = IsAlive(cam.colorRT) && cam.colorRT->Create();
    if (GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify CreateCamera color RT: id='{}' texture='{}' size={}x{} format={} created={}",
                       name, cam.textureName.value_or("<none>"), w, h, colorFormat.value__, BoolText(colorCreated));
    }
    if (!colorCreated) {
      ReleaseSecondaryCameraData(cam);
      return;
    }
    cam.camera->set_targetTexture(cam.colorRT);
    UnityEngine::Shader::SetGlobalTexture(cam.texturePropertyId.value(),
      static_cast<UnityEngine::Texture*>(cam.colorRT));
  }
  if (cam.depthTexturePropertyId.has_value()) {
    if (UnityEngine::SystemInfo::SupportsRenderTextureFormat(UnityEngine::RenderTextureFormat::Depth)) {
      cam.depthRT = UnityEngine::RenderTexture::New_ctor(w, h, 24, UnityEngine::RenderTextureFormat::Depth);
      bool depthCreated = IsAlive(cam.depthRT) && cam.depthRT->Create();
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify CreateCamera depth RT: id='{}' depthTexture='{}' size={}x{} format={} created={}",
                         name, cam.depthTextureName.value_or("<none>"), w, h,
                         UnityEngine::RenderTextureFormat::Depth.value__, BoolText(depthCreated));
      }
      if (depthCreated) {
        UnityEngine::Shader::SetGlobalTexture(cam.depthTexturePropertyId.value(),
          static_cast<UnityEngine::Texture*>(cam.depthRT));
      } else {
        ReleaseRenderTexture(cam.depthRT);
      }
    } else if (GetVivifyDebugLogging()) {
      PaperLogger.warn("Vivify CreateCamera depth RT skipped: Depth RenderTextureFormat unsupported id='{}'", name);
    }
  }
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify CreateCamera: id='{}' colorTexture='{}' depthTexture='{}' size={}x{} cameraDepth={} depthTextureMode={} clearFlags={} xrOcclusionMesh={}",
                     name,
                     cam.textureName.value_or("<none>"),
                     cam.depthTextureName.value_or("<none>"),
                     w,
                     h,
                     cam.camera->get_depth(),
                     cam.properties.depthTextureMode.has_value() ? cam.properties.depthTextureMode->value__ : -1,
                     cam.properties.clearFlags.has_value() ? cam.properties.clearFlags->value__ : -1,
                     BoolText(UnityEngine::XR::XRSettings::get_useOcclusionMesh()));
  }
  ApplyCameraProperties(cam.camera, cam.properties);
  _secondaryCameras[name] = std::move(cam);
}
std::optional<UnityEngine::DepthTextureMode> ParseDepthTextureModeName(std::string_view s) {
  if (s == "None") return UnityEngine::DepthTextureMode::None;
  if (s == "Depth") return UnityEngine::DepthTextureMode::Depth;
  if (s == "DepthNormals") return UnityEngine::DepthTextureMode::DepthNormals;
  if (s == "MotionVectors") return UnityEngine::DepthTextureMode::MotionVectors;
  return std::nullopt;
}
std::optional<UnityEngine::DepthTextureMode> ParseDepthTextureModeValue(rapidjson::Value const& value) {
  if (value.IsString()) {
    return ParseDepthTextureModeName(std::string_view(value.GetString(), value.GetStringLength()));
  }
  if (!value.IsArray()) {
    return std::nullopt;
  }

  int flags = 0;
  bool parsedAny = false;
  for (auto const& item : value.GetArray()) {
    if (!item.IsString()) continue;
    auto mode = ParseDepthTextureModeName(std::string_view(item.GetString(), item.GetStringLength()));
    if (!mode.has_value()) continue;
    flags |= mode->value__;
    parsedAny = true;
  }
  if (!parsedAny) return std::nullopt;
  return UnityEngine::DepthTextureMode(flags);
}
void ParseCameraPropertyData(rapidjson::Value const& json, CameraPropertyData& out) {
  if (auto* dtm = ReadValuePtr(json, "depthTextureMode")) {
    out.depthTextureMode = ParseDepthTextureModeValue(*dtm);
  }
  if (auto cf = ReadStringView(json, "clearFlags"))
    out.clearFlags = [&]() -> std::optional<UnityEngine::CameraClearFlags> {
      auto s = *cf;
      if (s == "Skybox") return UnityEngine::CameraClearFlags::Skybox;
      if (s == "Color" || s == "SolidColor") return UnityEngine::CameraClearFlags::Color;
      if (s == "Depth") return UnityEngine::CameraClearFlags::Depth;
      if (s == "Nothing") return UnityEngine::CameraClearFlags::Nothing;
      return std::nullopt;
    }();
  auto* bgVal = ReadValuePtr(json, "backgroundColor");
  if (bgVal != nullptr && bgVal->IsArray() && bgVal->Size() >= 3) {
    auto arr = bgVal->GetArray();
    float r = arr[0].GetFloat(), g = arr[1].GetFloat(), b = arr[2].GetFloat();
    float a = bgVal->Size() > 3 ? arr[3].GetFloat() : 1.0f;
    out.backgroundColor = UnityEngine::Color(r, g, b, a);
  }
  if (auto bp = ReadBool(json, "bloomPrePass")) out.bloomPrePass = *bp;
  if (auto me = ReadBool(json, "mainEffect")) out.mainEffect = *me;
  auto* culling = ReadValuePtr(json, "culling");
  if (culling != nullptr && culling->IsObject()) {
    bool const v2 = _currentBeatmapData != nullptr && _currentBeatmapData->v2orEarlier;
    out.cullingTracks = ReadTracks(*culling, v2);
    if (out.cullingTracks.empty()) {
      out.cullingTracks = ReadTracks(*culling, !v2);
    }
    if (auto whitelist = ReadBool(*culling, "whitelist")) {
      out.cullingWhitelist = *whitelist;
    }
    out.cullingMask = BuildTrackCullingMask(out.cullingTracks, out.cullingWhitelist);
  }
}
void ApplyCameraProperties(UnityEngine::Camera* camera, CameraPropertyData const& props) {
  if (camera == nullptr) return;
  if (props.depthTextureMode.has_value())
    camera->set_depthTextureMode(props.depthTextureMode.value());
  if (props.clearFlags.has_value())
    camera->set_clearFlags(props.clearFlags.value());
  if (props.backgroundColor.has_value())
    camera->set_backgroundColor(props.backgroundColor.value());
  if (props.cullingMask.has_value())
    camera->set_cullingMask(props.cullingMask.value());
}
void HandleSetCameraProperty(rapidjson::Value const& json) {
  auto id = ReadStringView(json, "id");
  std::string camId = id.has_value() ? std::string(*id) : "_Main";
  auto* propsVal = ReadValuePtr(json, "properties");
  if (propsVal == nullptr || !propsVal->IsObject()) return;
  CameraPropertyData props;
  ParseCameraPropertyData(*propsVal, props);
  if (GetDisableCreateCameraDepth()) {
    if (camId != "_Main") {
      if (GetVivifyDebugLogging()) {
        PaperLogger.info("Vivify SetCameraProperty skipped: CreateCamera/Depth isolation camera='{}'", camId);
      }
      return;
    }
    if (props.depthTextureMode.has_value() && GetVivifyDebugLogging()) {
      PaperLogger.info("Vivify SetCameraProperty depthTextureMode disabled: camera='{}' requested={}",
                       camId, props.depthTextureMode->value__);
    }
    props.depthTextureMode.reset();
  }
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify SetCameraProperty: camera='{}' depthTextureMode={} clearFlags={} hasBackground={} xrOcclusionMesh={}",
                     camId,
                     props.depthTextureMode.has_value() ? props.depthTextureMode->value__ : -1,
                     props.clearFlags.has_value() ? props.clearFlags->value__ : -1,
                     BoolText(props.backgroundColor.has_value()),
                     BoolText(UnityEngine::XR::XRSettings::get_useOcclusionMesh()));
  }
  _cameraProperties[camId] = props;
  if (camId == "_Main") {
    auto mainCam = UnityEngine::Camera::get_main();
    ApplyCameraProperties(mainCam, props);
  } else {
    auto it = _secondaryCameras.find(camId);
    if (it != _secondaryCameras.end()) {
      it->second.properties = props;
      ApplyCameraProperties(it->second.camera, props);
    }
  }
}
void HandleSetRenderingSettings(CustomJSONData::CustomEventData* customEventData,
                                rapidjson::Value const& json) {
  float duration = DurationBeatsToSeconds(ReadFloat(json, "duration").value_or(0.0f));
  Functions easing = ParseEasing(ReadStringView(json, "easing").value_or("easeLinear"));
  float startTime = customEventData->time;
  float songTime = CurrentSongTime();
  bool noDuration = duration <= 0.0f || startTime + duration <= songTime;
  std::vector<RenderSettingValue> settings;
  auto* rsVal = ReadValuePtr(json, "renderSettings");
  if (rsVal != nullptr && rsVal->IsObject()) {
    for (auto it = rsVal->MemberBegin(); it != rsVal->MemberEnd(); ++it) {
      std::string key = it->name.GetString();
      ParseAndApplyRenderSetting(key, it->value, settings, noDuration);
    }
  }
  auto* qsVal = ReadValuePtr(json, "qualitySettings");
  if (qsVal != nullptr && qsVal->IsObject()) {
    for (auto it = qsVal->MemberBegin(); it != qsVal->MemberEnd(); ++it) {
      std::string key = it->name.GetString();
      ParseAndApplyQualitySetting(key, it->value, settings, noDuration);
    }
  }
  if (!noDuration && !settings.empty()) {
    _renderSettingAnimations.emplace_back(ActiveRenderSettingAnimation{
      std::move(settings), startTime, duration, startTime + duration, static_cast<Functions>(easing)});
  }
  if (GetVivifyDebugLogging()) {
    PaperLogger.info("Vivify SetRenderingSettings: beat={} durationSeconds={} immediate={} animatedSettings={} hasRenderSettings={} hasQualitySettings={} xrOcclusionMesh={}",
                     customEventData->time,
                     duration,
                     BoolText(noDuration),
                     settings.size(),
                     BoolText(rsVal != nullptr && rsVal->IsObject()),
                     BoolText(qsVal != nullptr && qsVal->IsObject()),
                     BoolText(UnityEngine::XR::XRSettings::get_useOcclusionMesh()));
  }
}
void SaveRenderSetting(std::string const& name, RenderSettingKind kind) {
  for (auto const& s : _savedRenderSettings) { if (s.name == name) return; }
  SavedRenderSetting saved;
  saved.name = name;
  saved.kind = kind;
  if (name == "fog") saved.saved = UnityEngine::RenderSettings::get_fog();
  else if (name == "fogColor") saved.saved = UnityEngine::RenderSettings::get_fogColor();
  else if (name == "fogDensity") saved.saved = UnityEngine::RenderSettings::get_fogDensity();
  else if (name == "fogStartDistance") saved.saved = UnityEngine::RenderSettings::get_fogStartDistance();
  else if (name == "fogEndDistance") saved.saved = UnityEngine::RenderSettings::get_fogEndDistance();
  else if (name == "fogMode") saved.saved = static_cast<int>(UnityEngine::RenderSettings::get_fogMode());
  else if (name == "ambientIntensity") saved.saved = UnityEngine::RenderSettings::get_ambientIntensity();
  else if (name == "ambientLight") saved.saved = UnityEngine::RenderSettings::get_ambientLight();
  else if (name == "reflectionIntensity") saved.saved = UnityEngine::RenderSettings::get_reflectionIntensity();
  else if (name == "haloStrength") saved.saved = UnityEngine::RenderSettings::get_haloStrength();
  else if (name == "flareStrength") saved.saved = UnityEngine::RenderSettings::get_flareStrength();
  else if (name == "flareFadeSpeed") saved.saved = UnityEngine::RenderSettings::get_flareFadeSpeed();
  else if (name == "ambientSkyColor") saved.saved = UnityEngine::RenderSettings::get_ambientSkyColor();
  else if (name == "ambientEquatorColor") saved.saved = UnityEngine::RenderSettings::get_ambientEquatorColor();
  else if (name == "ambientGroundColor") saved.saved = UnityEngine::RenderSettings::get_ambientGroundColor();
  else if (name == "subtractiveShadowColor") saved.saved = UnityEngine::RenderSettings::get_subtractiveShadowColor();
  else if (name == "antiAliasing") saved.saved = UnityEngine::QualitySettings::get_antiAliasing();
  else return;
  _savedRenderSettings.push_back(std::move(saved));
}
void ApplyRenderSettingValue(std::string const& name, float val) {
  SaveRenderSetting(name, RenderSettingKind::Float);
  if (name == "fogDensity") UnityEngine::RenderSettings::set_fogDensity(val);
  else if (name == "fogStartDistance") UnityEngine::RenderSettings::set_fogStartDistance(val);
  else if (name == "fogEndDistance") UnityEngine::RenderSettings::set_fogEndDistance(val);
  else if (name == "ambientIntensity") UnityEngine::RenderSettings::set_ambientIntensity(val);
  else if (name == "reflectionIntensity") UnityEngine::RenderSettings::set_reflectionIntensity(val);
  else if (name == "haloStrength") UnityEngine::RenderSettings::set_haloStrength(val);
  else if (name == "flareStrength") UnityEngine::RenderSettings::set_flareStrength(val);
  else if (name == "flareFadeSpeed") UnityEngine::RenderSettings::set_flareFadeSpeed(val);
}
void ApplyRenderSettingColor(std::string const& name, UnityEngine::Color val) {
  SaveRenderSetting(name, RenderSettingKind::Color);
  if (name == "fogColor") UnityEngine::RenderSettings::set_fogColor(val);
  else if (name == "ambientLight") UnityEngine::RenderSettings::set_ambientLight(val);
  else if (name == "ambientSkyColor") UnityEngine::RenderSettings::set_ambientSkyColor(val);
  else if (name == "ambientEquatorColor") UnityEngine::RenderSettings::set_ambientEquatorColor(val);
  else if (name == "ambientGroundColor") UnityEngine::RenderSettings::set_ambientGroundColor(val);
  else if (name == "subtractiveShadowColor") UnityEngine::RenderSettings::set_subtractiveShadowColor(val);
}
void ApplyRenderSettingBool(std::string const& name, bool val) {
  SaveRenderSetting(name, RenderSettingKind::Bool);
  if (name == "fog") UnityEngine::RenderSettings::set_fog(val);
}
void ApplyRenderSettingInt(std::string const& name, int val) {
  SaveRenderSetting(name, RenderSettingKind::Int);
  if (name == "antiAliasing") UnityEngine::QualitySettings::set_antiAliasing(val);
  else if (name == "fogMode") UnityEngine::RenderSettings::set_fogMode(static_cast<UnityEngine::FogMode>(val));
  else if (name == "ambientMode") UnityEngine::RenderSettings::set_ambientMode(
    static_cast<UnityEngine::Rendering::AmbientMode>(val));
  else if (name == "reflectionBounces") UnityEngine::RenderSettings::set_reflectionBounces(val);
  else if (name == "defaultReflectionResolution")
    UnityEngine::RenderSettings::set_defaultReflectionResolution(val);
}
void ParseAndApplyRenderSetting(std::string const& key, rapidjson::Value const& val,
                                std::vector<RenderSettingValue>& out, bool immediate) {
  if (key == "fogColor" || key == "ambientLight" || key == "ambientSkyColor" ||
      key == "ambientEquatorColor" || key == "ambientGroundColor" ||
      key == "subtractiveShadowColor") {
    if (val.IsArray() && val.Size() >= 3 && val[0].IsNumber() && val[1].IsNumber() && val[2].IsNumber()) {
      auto a = val.GetArray();
      UnityEngine::Color c(a[0].GetFloat(), a[1].GetFloat(), a[2].GetFloat(),
                           val.Size() > 3 ? a[3].GetFloat() : 1.0f);
      ApplyRenderSettingColor(key, c);
    }
    return;
  }
  if (val.IsString()) {
    return;
  }
  if (key == "fog" || key == "realtimeReflectionProbes" || key == "softParticles") {
    bool b = false;
    if (val.IsBool()) b = val.GetBool();
    else if (val.IsNumber()) b = val.GetFloat() != 0.0f;
    ApplyRenderSettingBool(key, b);
    return;
  }
  if (val.IsNumber()) {
    float f = val.GetFloat();
    if (key == "fogMode" || key == "ambientMode" || key == "antiAliasing" ||
        key == "pixelLightCount" || key == "reflectionBounces" ||
        key == "defaultReflectionResolution" || key == "defaultReflectionMode" ||
        key == "shadowCascades" || key == "shadowResolution" || key == "shadows" ||
        key == "shadowProjection" || key == "shadowmaskMode" ||
        key == "anisotropicFiltering") {
      ApplyRenderSettingInt(key, static_cast<int>(f));
    } else {
      ApplyRenderSettingValue(key, f);
      if (!immediate) {
        RenderSettingValue rsv;
        rsv.name = key;
        rsv.kind = RenderSettingKind::Float;
        rsv.value = f;
        out.push_back(std::move(rsv));
      }
    }
  }
}
void ParseAndApplyQualitySetting(std::string const& key, rapidjson::Value const& val,
                                 std::vector<RenderSettingValue>& out, bool immediate) {
  ParseAndApplyRenderSetting(key, val, out, immediate);
}
void UpdateRenderSettingAnimations() {
  if (_renderSettingAnimations.empty()) return;
  float const songTime = CurrentSongTime();
  auto write = _renderSettingAnimations.begin();
  for (auto read = _renderSettingAnimations.begin(); read != _renderSettingAnimations.end(); ++read) {
    float const progress = AnimationProgress(read->startTime, read->duration, read->easing, songTime);
    for (auto const& s : read->settings) {
      if (s.kind == RenderSettingKind::Float && std::holds_alternative<PointDefinitionW>(s.value)) {
        ApplyRenderSettingValue(s.name, std::get<PointDefinitionW>(s.value).InterpolateLinear(progress));
      }
    }
    if (songTime < read->endTime) {
      if (write != read) *write = std::move(*read);
      ++write;
    }
  }
  _renderSettingAnimations.erase(write, _renderSettingAnimations.end());
}
void RestoreRenderSettings() {
  for (auto const& s : _savedRenderSettings) {
    if (s.kind == RenderSettingKind::Float && std::holds_alternative<float>(s.saved))
      ApplyRenderSettingValue(s.name, std::get<float>(s.saved));
    else if (s.kind == RenderSettingKind::Color && std::holds_alternative<UnityEngine::Color>(s.saved))
      ApplyRenderSettingColor(s.name, std::get<UnityEngine::Color>(s.saved));
    else if (s.kind == RenderSettingKind::Bool && std::holds_alternative<bool>(s.saved))
      ApplyRenderSettingBool(s.name, std::get<bool>(s.saved));
    else if (s.kind == RenderSettingKind::Int && std::holds_alternative<int>(s.saved))
      ApplyRenderSettingInt(s.name, std::get<int>(s.saved));
  }
}
void HandleAssignObjectPrefab(CustomJSONData::CustomEventData*, rapidjson::Value const& json) {
  bool const v2 = _currentBeatmapData != nullptr && _currentBeatmapData->v2orEarlier;
  bool const additive = [&json]() {
    auto loadMode = ReadStringView(json, "loadMode");
    return loadMode.has_value() && NormalizeAssetKey(*loadMode) == "additive";
  }();

  auto applyAsset = [this, additive](rapidjson::Value const& objVal,
                                    std::string_view objType,
                                    AssignedPrefabKind kind,
                                    std::string_view key,
                                    std::vector<TrackW> tracks,
                                    std::optional<int> saberType = std::nullopt) {
    auto asset = ReadAssetValue(objVal, key);
    if (asset.state == AssetValueState::Missing) return;
    if (asset.state == AssetValueState::Null) {
      ClearAssignedPrefabs(objType, kind, saberType, &tracks);
      return;
    }
    if (!additive) {
      ClearAssignedPrefabs(objType, kind, saberType, &tracks);
    }
    AddAssignedPrefab(objType, kind, std::move(asset.value), std::move(tracks), additive, saberType);
  };
  auto applyTrail = [this, additive](rapidjson::Value const& objVal, int saberType) {
    auto asset = ReadAssetValue(objVal, "trailAsset");
    if (asset.state == AssetValueState::Missing) return;
    if (asset.state == AssetValueState::Null) {
      ClearAssignedPrefabs("saber", AssignedPrefabKind::Trail, saberType);
      return;
    }
    if (!additive) {
      ClearAssignedPrefabs("saber", AssignedPrefabKind::Trail, saberType);
    }
    AddAssignedTrail(std::move(asset.value),
                     additive,
                     saberType,
                     ReadVector3(objVal, "trailTopPos"),
                     ReadVector3(objVal, "trailBottomPos"),
                     ReadFloat(objVal, "trailDuration"),
                     ReadInt(objVal, "trailSamplingFrequency"),
                     ReadInt(objVal, "trailGranularity"));
  };

  auto processTrackedObject = [&](std::string_view objType) {
    auto* objVal = ReadValuePtr(json, objType);
    if (objVal == nullptr || !objVal->IsObject()) return;
    auto tracks = ReadTracks(*objVal, v2);
    applyAsset(*objVal, objType, AssignedPrefabKind::Object, "asset", tracks);
    applyAsset(*objVal, objType, AssignedPrefabKind::Debris, "debrisAsset", tracks);
  };

  if (auto* colorNotes = ReadValuePtr(json, "colorNotes"); colorNotes != nullptr && colorNotes->IsObject()) {
    auto tracks = ReadTracks(*colorNotes, v2);
    applyAsset(*colorNotes, "colorNotes", AssignedPrefabKind::Object, "asset", tracks);
    applyAsset(*colorNotes, "colorNotes", AssignedPrefabKind::AnyDirectionObject, "anyDirectionAsset", tracks);
    applyAsset(*colorNotes, "colorNotes", AssignedPrefabKind::Debris, "debrisAsset", tracks);
  }
  processTrackedObject("bombNotes");
  processTrackedObject("burstSliders");
  processTrackedObject("burstSliderElements");

  bool saberChanged = false;
  if (auto* saber = ReadValuePtr(json, "saber"); saber != nullptr && saber->IsObject()) {
    auto type = ReadStringView(*saber, "type");
    std::vector<int> saberTypes;
    std::string normalizedType = type.has_value() ? NormalizeAssetKey(*type) : "both";
    if (normalizedType == "both") {
      saberTypes = {0, 1};
    } else if (normalizedType == "left" || normalizedType == "sabera") {
      saberTypes = {0};
    } else if (normalizedType == "right" || normalizedType == "saberb") {
      saberTypes = {1};
    }
    if (!saberTypes.empty()) {
      if (!additive) ClearAssignedPrefabs("saber");
      for (int saberType : saberTypes) {
        applyAsset(*saber, "saber", AssignedPrefabKind::Object, "asset", {}, saberType);
        applyTrail(*saber, saberType);
      }
      saberChanged = true;
    }
  }
  if (saberChanged) {
    ApplySaberVisualsToActive();
  }
}

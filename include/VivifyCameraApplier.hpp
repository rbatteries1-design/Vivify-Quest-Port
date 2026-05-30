#pragma once

#include "custom-types/shared/macros.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/RenderTexture.hpp"

DECLARE_CLASS_CODEGEN(Vivify, CameraApplier, UnityEngine::MonoBehaviour) {
  DECLARE_DEFAULT_CTOR();
  DECLARE_SIMPLE_DTOR();
  DECLARE_INSTANCE_METHOD(void, OnRenderImage, UnityEngine::RenderTexture* src, UnityEngine::RenderTexture* dest);
};

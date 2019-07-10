/*
 * Copyright (c) 2019 WangBin <wbsecg1 at gmail.com>
 */
#pragma once
#include "ugs/RenderLoop.h"
#include <EGL/egl.h>
#include <memory>

using namespace UGS_NS;
class EGLRenderLoop final : public RenderLoop
{
protected:
  void* createRenderContext(PlatformSurface* surface) override;
  bool destroyRenderContext(PlatformSurface* surface, void* ctx) override;
  bool activateRenderContext(PlatformSurface* surface, void* ctx) override;
  bool submitRenderContext(PlatformSurface* surface, void* ctx) override;
private:
};
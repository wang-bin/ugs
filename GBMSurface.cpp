/*
 * Copyright (c) 2017-2019 WangBin <wbsecg1 at gmail.com>
 */
#include "ugs/PlatformSurface.h"
#include <iostream>
extern "C" {
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <fcntl.h>
#include <unistd.h>
}
// symbols does not exist on raspian 7(libgbm 8.0.5-4+deb7u2+rpi)
_Pragma("weak gbm_surface_create")
_Pragma("weak gbm_surface_destroy")
_Pragma("weak gbm_surface_release_buffer")
_Pragma("weak gbm_surface_lock_front_buffer")
_Pragma("weak gbm_bo_get_stride")

// make the whole library weak so that it(and dependencies)'s not required by rpath-link (ld.bfd), but need to preload at runtime if link with --as-needed
UGS_NS_BEGIN
class GBMSurface final: public PlatformSurface
{
public:
    GBMSurface();
    ~GBMSurface() override;
    void* nativeResource() const override { return dev_;}
    void submit() override;
private:
    int drm_fd_ = 0;
    drmModeConnector *connector_ = nullptr;
    drmModeModeInfo mode_ = {};
    drmModeCrtc *crtc_ = nullptr;
    struct gbm_device *dev_ = nullptr;
    struct gbm_surface *surf_ = nullptr;
    struct gbm_bo *bo_ = nullptr;
    uint32_t fb_ = 0;
};

PlatformSurface* create_gbm_surface(void*) { return new GBMSurface(); }

int get_drm_fd(int card)
{
    char dev_path[128]{};
    snprintf(dev_path, sizeof(dev_path), DRM_DEV_NAME, DRM_DIR_NAME, card);
    return open(dev_path, O_RDWR|O_CLOEXEC);
}

int get_drm_fd()
{
    int card = 0;
    char* n = getenv("DRM_NUM");
    if (n)
        card = atoi(n);
    else
        std::clog << "env var DRM_NUM is not set, assume it's 0" << std::endl;
    return get_drm_fd(card); 
}

drmModeConnector* get_connector(int fd, drmModeRes* res)
{
    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
        if (!c)
            continue;
        if (c->connection == DRM_MODE_CONNECTED && c->count_modes > 0)
            return c;
        drmModeFreeConnector(c);
    }
    return nullptr;
}

GBMSurface::GBMSurface() : PlatformSurface(Type::GBM)
{
    if (!gbm_surface_create)
        return;
    drm_fd_ = get_drm_fd();
    if (drm_fd_ < 0) {
        std::clog << "failed to open drm card" << std::endl;
        return;
    }
    drmModeRes *res = drmModeGetResources(drm_fd_);
    connector_ = get_connector(drm_fd_, res);
    drmModeFreeResources(res);
    if (!connector_) {
        std::clog << "failed to get connector" << std::endl;
        return;
    }
    mode_ = connector_->modes[0]; // 
    drmModeEncoder *enc = drmModeGetEncoder(drm_fd_, connector_->encoder_id);
    if (!enc) {
        std::clog << "failed to get encoder" << std::endl;
        return;
    }
    crtc_ = drmModeGetCrtc(drm_fd_, enc->crtc_id);
    drmModeFreeEncoder(enc);
    if (!crtc_) {
        std::clog << "failed to get crtc" << std::endl;
        return;
    }
    dev_ = gbm_create_device(drm_fd_);
    surf_ = gbm_surface_create(dev_, mode_.hdisplay, mode_.vdisplay, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    resetNativeHandle(surf_);
}

GBMSurface::~GBMSurface()
{
    drmModeSetCrtc(drm_fd_, crtc_->crtc_id, crtc_->buffer_id, crtc_->x, crtc_->y, &connector_->connector_id, 1, &crtc_->mode);
    drmModeFreeCrtc(crtc_);
    if (connector_)
        drmModeFreeConnector(connector_);
    if (bo_) {
        drmModeRmFB(drm_fd_, fb_);
        gbm_surface_release_buffer(surf_, bo_);
    }
    if (surf_)
        gbm_surface_destroy(surf_);
    if (dev_)
        gbm_device_destroy(dev_);
    ::close(drm_fd_);
}

void GBMSurface::submit()
{
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(surf_);
	uint32_t handle = gbm_bo_get_handle(bo).u32;
	uint32_t pitch = gbm_bo_get_stride(bo);
	uint32_t fb = 0;
	drmModeAddFB(drm_fd_, gbm_bo_get_width(bo), gbm_bo_get_height(bo), 32, 32, pitch, handle, &fb);
	drmModeSetCrtc(drm_fd_, crtc_->crtc_id, fb, 0, 0, &connector_->connector_id, 1, &mode_);
	if (bo_) {
        // drmModePageFlip, wait flip
		drmModeRmFB(drm_fd_, fb_); // TODO: rm previous
		gbm_surface_release_buffer(surf_, bo_);
	}
	bo_ = bo;
	fb_ = fb;
}
UGS_NS_END

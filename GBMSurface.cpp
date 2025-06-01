/*
 * Copyright (c) 2017-2025 WangBin <wbsecg1 at gmail.com>

// https://manpages.debian.org/jessie/libdrm-dev/drm-kms.7.en.html
// run `sudo service gdm/lightdm stop` to test gbm

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
_Pragma("weak drmGetDevices2") // since 2016

// make the whole library weak so that it(and dependencies)'s not required by rpath-link (ld.bfd), but need to preload at runtime if link with --as-needed
UGS_NS_BEGIN
using namespace std;

static uint32_t fourcc_value(const char* name) {
    return uint32_t(name[0]) | (uint32_t(name[1]) << 8) | (uint32_t(name[2]) << 16) | (uint32_t(name[3]) << 24);
}

class GBMSurface final: public PlatformSurface
{
public:
    GBMSurface();
    ~GBMSurface() override;
    void* nativeResource() const override { return dev_;}
    void submit() override;
    bool size(int *w, int *h) const override {
        if (w)
            *w = mode_.hdisplay;
        if (h)
            *h = mode_.vdisplay;
        return true;
    }
private:
    bool initFdConnector(int card);

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


drmModeConnector* get_connector(int fd)
{
    auto res = drmModeGetResources(fd);
    if (!res)
        return nullptr;
    for (int i = 0; i < res->count_connectors; ++i) {
        if (auto c = drmModeGetConnector(fd, res->connectors[i])) {
            if (c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
                drmModeFreeResources(res);
                return c;
            }
            drmModeFreeConnector(c);
        }
    }
    drmModeFreeResources(res);
    return nullptr;
}

bool GBMSurface::initFdConnector(int card)
{
    drmDevice *devs[DRM_MAX_MINOR] = {};
    int count = 0;
    if (drmGetDevices2) {
        count = drmGetDevices2(0, devs, std::size(devs));
    } else {
        count = drmGetDevices(devs, std::size(devs));
    }
    if (count <= 0) {
        clog << "drmGetDevices error" << endl;
        return false;
    }
    if (card >= count) {
        clog << "invalid DRM card number. max: " + std::to_string(count) << endl;
        return false;
    }
    for (int i = std::max<int>(card, 0); i < count; ++i) {
        drmDevice *dev = devs[i];
        if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY))) {
            clog << "no primary node for DRM card " + std::to_string(i) << endl;
            if (card >= 0) // set by user
                return false;
            continue;
        }
        const char *card_path = dev->nodes[DRM_NODE_PRIMARY];
        clog << "testing DRM card " + std::to_string(i) + ", path: " + card_path << endl;
        if (auto fd = open(card_path, O_RDWR|O_CLOEXEC); fd >= 0) {
            // drmIsKMS(fd)
            if (auto connector = get_connector(fd)) {
                drm_fd_ = fd;
                connector_ = connector;
                clog << "select DRM card " + std::to_string(i) + ", path: " + card_path << endl;
                return true;
            } else {
                clog << "DRM card " + std::to_string(i) + ": no connector " << endl;
                if (card >= 0)
                    return false;
            }
            ::close(fd);
        }
    }
    return false;
}

static double get_Hz(const drmModeModeInfo& mode)
{
    double rate = mode.clock * 1000.0 / mode.htotal / mode.vtotal;
    if (mode.flags & DRM_MODE_FLAG_INTERLACE)
        rate *= 2.0;
    return rate;
}

GBMSurface::GBMSurface() : PlatformSurface(Type::GBM)
{
    if (!gbm_surface_create)
        return;
    char* env = getenv("DRM_CARD");
    if (!initFdConnector(env ? atoi(env) : -1)) {
        return;
    }
    // crtc
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
    // mode
    mode_ = connector_->modes[0];
    env = getenv("DRM_MODE");
    const int midx = env ? atoi(env) : 0;
    mode_ = connector_->modes[midx];
    for (unsigned i = 0; i < connector_->count_modes; ++i) {
        const auto& m = connector_->modes[i];
        // m.type & DRM_MODE_TYPE_PREFERRED ?
        clog << "  Mode " + std::to_string(i) + " :" + m.name + " - " + std::to_string(m.hdisplay) + "x" + std::to_string(m.hdisplay) + "@" + std::to_string(get_Hz(m)) + "Hz" + (i == midx ? " - Selected" : "") << endl;
    }
    dev_ = gbm_create_device(drm_fd_);
    // ARGB8888 for es and XRGB for desktop? https://gitlab.freedesktop.org/xorg/xserver/-/merge_requests/934
    // TODO: GBM_FORMAT_XBGR8888, XBGR2101010, XRGB2101010
    // rk3588 debian11 seems only supports AR24 in EGLConfig(EGL_NATIVE_VISUAL_ID)
    auto fmt = GBM_FORMAT_ARGB8888; // AR24
    if (const auto env = getenv("GBM_FORMAT")) {
        fmt = fourcc_value(env);
    }
    surf_ = gbm_surface_create(dev_, mode_.hdisplay, mode_.vdisplay, fmt, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
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
	const uint32_t handle = gbm_bo_get_handle(bo).u32;
	const uint32_t pitch = gbm_bo_get_stride(bo);
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

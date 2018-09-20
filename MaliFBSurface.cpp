/*
 * Copyright (c) 2016-2018 WangBin <wbsecg1 at gmail.com>
 * Universal Graphics Surface
 * Source code: https://github.com/wang-bin/ugs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// https://github.com/libretro/RetroArch/blob/master/gfx/drivers_context/mali_fbdev_ctx.c
#if defined(__arm__) && defined(__linux__)
#include "ugs/PlatformSurface.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <iostream>

UGS_NS_BEGIN
typedef struct fbdev_window
{
	unsigned short width;
	unsigned short height;
} fbdev_window;

class MaliFBSurface final : public PlatformSurface
{
public:
    MaliFBSurface();
    ~MaliFBSurface() override {
        if (!libmali_)
            return;
        dlclose(libmali_);
        // clear framebuffer and set cursor on again ?
    }
private:
    fbdev_window win_;
    void* libmali_ = nullptr; // egl and gles are simply symbolic links to libMali
};

MaliFBSurface::MaliFBSurface() : PlatformSurface()
{
    std::clog << "try to create mali fbdev window" << std::endl;
    libmali_ = ::dlopen("libMali.so", RTLD_LAZY|RTLD_GLOBAL); // not visible?
    if (!libmali_)
        return;
    void* f = dlsym(libmali_, "DRI2Connect");
    if (f) {
        std::clog << "mali for X11 is loaded. skip creating mali fbdev window" << std::endl;
        return;
    }
    struct fb_var_screeninfo vinfo;
    int fd = ::open("/dev/fb0", O_RDWR);
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
       std::clog << "failed to open framebuffer device" << std::endl;
       return;
    }
    ::close(fd);
    win_.width = vinfo.xres;
    win_.height = vinfo.yres;
    resize(win_.width, win_.height); // resize event
    resetNativeHandle(&win_); // virtual onNativeHandleChanged()!!!
}

PlatformSurface* create_malifb_surface(void*) {
    if (access("/dev/mali", F_OK) == -1) // required? check loaded modules?
        return nullptr;
        // file exists. R_OK/W_OK/X_OK: read/write/execute permission
    return new MaliFBSurface();
}
UGS_NS_END
#endif // defined(__arm__) && defined(__linux__)
/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>/<binwang at pptv.com>
 */
#include "ugsurface/PlatformSurface.h"
#include <cassert>
#include <WF/wfc.h>
#include <bcm_host.h>

UGSURFACE_NS_BEGIN
#define NO_ATTRIBUTES nullptr

class OpenWFCSurface final : public PlatformSurface
{
public:
    OpenWFCSurface() {
        bcm_host_init(); //?
        wfcdev_ = wfcCreateDevice(WFC_DEFAULT_DEVICE_ID, NO_ATTRIBUTES);
        assert(wfcdev_ && "null wfc device");
        wfcctx_ = wfcCreateOnScreenContext(wfcdev_, 0, NO_ATTRIBUTES);
        assert(wfcctx_ && "null wfc on screen context");
        const WFCint w = wfcGetContextAttribi(wfcdev_, wfcctx_, WFC_CONTEXT_TARGET_WIDTH);
        const WFCint h = wfcGetContextAttribi(wfcdev_, wfcctx_, WFC_CONTEXT_TARGET_HEIGHT);
        printf("WFC context size: %dx%d\n", w, h);fflush(stdout);

#if 0
        WFCSource source = wfcCreateSourceFromStream(wfcdev_, wfcctx_, (WFCNativeStreamType)wfcelement_, NO_ATTRIBUTES);
        assert(source != WFC_INVALID_HANDLE && "invalid wfc source");
#endif
        wfcelement_ = wfcCreateElement(wfcdev_, wfcctx_, NO_ATTRIBUTES);
        assert(wfcelement_ != WFC_INVALID_HANDLE && "invalid wfc element");
        wfcInsertElement(wfcdev_, wfcelement_, WFC_INVALID_HANDLE);
        /// wfcGetError(wfcdev_);
        const WFCint src[] = {0, 0, w, h};
        const WFCint dst[] = {0, 0, w, h};
        wfcSetElementAttribiv(wfcdev_, wfcelement_, WFC_ELEMENT_SOURCE_RECTANGLE, 4, src);
        wfcSetElementAttribiv(wfcdev_, wfcelement_, WFC_ELEMENT_DESTINATION_RECTANGLE, 4, dst);
        
        // Create and attach source // Stream and element handles given same value.
        WFCSource source = wfcCreateSourceFromStream(wfcdev_, wfcctx_, (WFCNativeStreamType)wfcelement_, NO_ATTRIBUTES);
        assert(source != WFC_INVALID_HANDLE && "invalid wfc source");
        wfcSetElementAttribi(wfcdev_, wfcelement_, WFC_ELEMENT_SOURCE, source);

        wfcCommit(wfcdev_, wfcctx_, WFC_TRUE);
        wfcActivate(wfcdev_, wfcctx_);
        dwin_ = new EGL_DISPMANX_WINDOW_T;
        dwin_->width = 800;
        dwin_->height = 480;
        dwin_->element = wfcelement_;
        resetNativeHandle(dwin_);
    }

    ~OpenWFCSurface() {
        delete dwin_;
    }

    void resize(int w, int h) override {
    }
    void close() override {
    }
private:
    WFCDevice wfcdev_ = WFC_INVALID_HANDLE;
    WFCContext wfcctx_ = WFC_INVALID_HANDLE;
    WFCElement wfcelement_ = WFC_INVALID_HANDLE;
    EGL_DISPMANX_WINDOW_T *dwin_ = nullptr;
};

PlatformSurface* create_wfc() {
    return new OpenWFCSurface();
}

UGSURFACE_NS_END

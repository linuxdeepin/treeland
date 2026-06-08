// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wtextureproviderprovider.h"
#include "woutputrenderwindow.h"
#include "private/wglobal_p.h"

#include <rhi/qrhi.h>
#include <private/qquickwindow_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
Q_LOGGING_CATEGORY(qLcTextureProvider, "waylib.server.texture.provider")

class Q_DECL_HIDDEN WTextureCapturerPrivate : public WObjectPrivate
{
public:
    W_DECLARE_PUBLIC(WTextureCapturer)
    explicit WTextureCapturerPrivate(WTextureCapturer *qq, WTextureProviderProvider *p)
        : WObjectPrivate(qq)
        , provider(p)
        , renderWindow(p->outputRenderWindow())
    {}

    QPromise<QImage> imgPromise;
    WTextureProviderProvider *const provider;
    WOutputRenderWindow *const renderWindow;
};

static void cleanupRbResult(void *rbResult)
{
    delete reinterpret_cast<QRhiReadbackResult*>(rbResult);
}

WTextureCapturer::WTextureCapturer(WTextureProviderProvider *provider, QObject *parent)
    : QObject(parent)
    , WObject(*new WTextureCapturerPrivate(this, provider))
{

}

QFuture<QImage> WTextureCapturer::grabToImage()
{
    W_D(WTextureCapturer);
    auto future = d->imgPromise.future();
    moveToThread(QQuickWindowPrivate::get(d->renderWindow)->context->thread());
    if (d->renderWindow->inRendering()) {
        connect(d->renderWindow, &WOutputRenderWindow::renderEnd, this, &WTextureCapturer::doGrabToImage, Qt::AutoConnection);
    } else {
        // FIXME What if a new render process start before this job is executed?
        QMetaObject::invokeMethod(this, &WTextureCapturer::doGrabToImage, Qt::AutoConnection);
    }
    return future;
}

void WTextureCapturer::doGrabToImage()
{
    W_D(WTextureCapturer);
    if (d->imgPromise.isCanceled())
        return;
    d->imgPromise.start();
    WSGTextureProvider *textureProvider = d->provider->wTextureProvider();
    if (textureProvider && textureProvider->texture() && textureProvider->texture()->rhiTexture()) {
        // Perform rhi texture read back
        auto texture = textureProvider->texture()->rhiTexture();
        qCInfo(qLcTextureProvider) << "Perform rhi texture read back for texture" << texture;
        QRhiReadbackResult *rbResult = new QRhiReadbackResult;
        QRhiCommandBuffer *cb;
        d->renderWindow->rhi()->beginOffscreenFrame(&cb);
        auto ub = d->renderWindow->rhi()->nextResourceUpdateBatch();
        cb->beginComputePass(ub);
        QRhiReadbackDescription rbd(texture);
        ub->readBackTexture(rbd, rbResult);
        cb->endComputePass(ub);
        auto frameOpResult = d->renderWindow->rhi()->endOffscreenFrame();
        if (frameOpResult == QRhi::FrameOpSuccess) {
            d->imgPromise.addResult(QImage(reinterpret_cast<const uchar *>(rbResult->data.constData()),
                                       rbResult->pixelSize.width(),
                                       rbResult->pixelSize.height(),
                                       QImage::Format_RGBA8888_Premultiplied,
                                       cleanupRbResult,
                                       rbResult)); // TODO: Automatically get data format.
        } else {
            d->imgPromise.setException(std::make_exception_ptr(std::runtime_error("Offscreen frame operation failed.")));
        }
    } else {
        d->imgPromise.setException(std::make_exception_ptr(std::runtime_error("Texture provider is not valid.")));
    }
    d->imgPromise.finish();
}

WAYLIB_SERVER_END_NAMESPACE

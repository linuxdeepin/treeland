// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputhelper.h"
#include "wrenderhelper.h"
#include "woutput.h"
#include "platformplugin/types.h"

#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwswapchain.h>
#include <qwbuffer.h>
#include <platformplugin/qwlrootswindow.h>
#include <platformplugin/qwlrootsintegration.h>
#include <platformplugin/qwlrootscreen.h>

#include <QWindow>
#include <QQuickWindow>
#ifndef QT_NO_OPENGL
#include <QOpenGLContext>
#endif
#include <private/qquickwindow_p.h>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputHelperPrivate : public WObjectPrivate
{
public:
    WOutputHelperPrivate(WOutput *output, WOutputHelper *qq)
        : WObjectPrivate(qq)
        , output(output)
        , outputWindow(new QW::Window)
        , renderable(false)
        , contentIsDirty(false)
        , needsFrame(false)
    {
        outputWindow->QObject::setParent(qq);
        outputWindow->setScreen(QWlrootsIntegration::instance()->getScreenFrom(output)->screen());
        outputWindow->create();

        QObject::connect(qwoutput(), &QWOutput::frame, qq, [this] {
            on_frame();
        });
        QObject::connect(qwoutput(), &QWOutput::needsFrame, qq, [this] {
            setNeedsFrame(true);
            qwoutput()->QWOutput::scheduleFrame();
        });
        QObject::connect(qwoutput(), &QWOutput::damage, qq, [this] {
            on_damage();
        });
        QObject::connect(output, &WOutput::modeChanged, qq, [this] {
            if (renderHelper)
                renderHelper->setSize(this->output->size());
            qpaWindow()->setBuffer(nullptr);
        }, Qt::QueuedConnection); // reset buffer on later, because it's rendering

        // In call the connect for 'frame' signal before, maybe the wlr_output object is already \
        // emit the signal, so we should suppose the renderable is true in order that ensure can \
        // render on the next time
        renderable = true;
    }

    inline QWOutput *qwoutput() const {
        return output->handle();
    }

    inline QWRenderer *renderer() const {
        return output->renderer();
    }

    inline QWSwapchain *swapchain() const {
        return output->swapchain();
    }

    inline QWlrootsOutputWindow *qpaWindow() const {
        return static_cast<QWlrootsOutputWindow*>(outputWindow->handle());
    }

    void setRenderable(bool newValue);
    void setContentIsDirty(bool newValue);
    void setNeedsFrame(bool newNeedsFrame);

    void on_frame();
    void on_damage();

    QWBuffer *acquireBuffer(int *bufferAge);

    inline bool makeCurrent(QWBuffer *buffer, QOpenGLContext *context) {
        qpaWindow()->setBuffer(buffer);
        bool ok = false;
#ifndef QT_NO_OPENGL
        if (context) {
            ok = context->makeCurrent(outputWindow);
        } else
#endif
        {
            ok = qpaWindow()->attachRenderer();
        }

        if (!ok)
            buffer->unlock();

        return ok;
    }

    inline void doneCurrent(QOpenGLContext *context) {
#ifndef QT_NO_OPENGL
        if (context) {
            context->doneCurrent();
        } else
#endif
        {
            qpaWindow()->detachRenderer();
        }
    }

    inline void update() {
        setContentIsDirty(true);
    }

    W_DECLARE_PUBLIC(WOutputHelper)
    WOutput *output;
    QWindow *outputWindow;
    WRenderHelper *renderHelper = nullptr;

    uint renderable:1;
    uint contentIsDirty:1;
    uint needsFrame:1;
};

void WOutputHelperPrivate::setRenderable(bool newValue)
{
    if (renderable == newValue)
        return;
    renderable = newValue;
    Q_EMIT q_func()->renderableChanged();
}

void WOutputHelperPrivate::setContentIsDirty(bool newValue)
{
    if (contentIsDirty == newValue)
        return;
    contentIsDirty = newValue;
    Q_EMIT q_func()->contentIsDirtyChanged();
}

void WOutputHelperPrivate::setNeedsFrame(bool newNeedsFrame)
{
    if (needsFrame == newNeedsFrame)
        return;
    needsFrame = newNeedsFrame;
    Q_EMIT q_func()->needsFrameChanged();
}

void WOutputHelperPrivate::on_frame()
{
    setRenderable(true);
    Q_EMIT q_func()->requestRender();
}

void WOutputHelperPrivate::on_damage()
{
    setContentIsDirty(true);
    Q_EMIT q_func()->damaged();
}

QWBuffer *WOutputHelperPrivate::acquireBuffer(int *bufferAge)
{
    bool ok = wlr_output_configure_primary_swapchain(qwoutput()->handle(),
                                                     &qwoutput()->handle()->pending,
                                                     &qwoutput()->handle()->swapchain);
    if (!ok)
        return nullptr;
    QWBuffer *newBuffer = swapchain()->acquire(bufferAge);
    return newBuffer;
}

WOutputHelper::WOutputHelper(WOutput *output, QObject *parent)
    : QObject(parent)
    , WObject(*new WOutputHelperPrivate(output, this))
{

}

WOutput *WOutputHelper::output() const
{
    W_DC(WOutputHelper);
    return d->output;
}

QWindow *WOutputHelper::outputWindow() const
{
    W_DC(WOutputHelper);
    return d->outputWindow;
}

std::pair<QWBuffer*, QQuickRenderTarget> WOutputHelper::acquireRenderTarget(QQuickRenderControl *rc, int *bufferAge)
{
    W_D(WOutputHelper);

    QWBuffer *buffer = d->acquireBuffer(bufferAge);
    if (!buffer)
        return {};

    if (!d->renderHelper) {
        d->renderHelper = new WRenderHelper(d->renderer(), this);
        d->renderHelper->setSize(d->output->size());
    }
    auto rt = d->renderHelper->acquireRenderTarget(rc, buffer);
    if (rt.isNull()) {
        buffer->unlock();
        return {};
    }

    return {buffer, rt};
}

std::pair<QWBuffer*, QQuickRenderTarget> WOutputHelper::lastRenderTarget()
{
    W_DC(WOutputHelper);
    if (!d->renderHelper)
        return {nullptr, {}};

    return d->renderHelper->lastRenderTarget();
}

bool WOutputHelper::renderable() const
{
    W_DC(WOutputHelper);
    return d->renderable;
}

bool WOutputHelper::contentIsDirty() const
{
    W_DC(WOutputHelper);
    return d->contentIsDirty;
}

bool WOutputHelper::needsFrame() const
{
    W_DC(WOutputHelper);
    return d->needsFrame;
}

bool WOutputHelper::makeCurrent(QWBuffer *buffer, QOpenGLContext *context)
{
    W_D(WOutputHelper);
    return d->makeCurrent(buffer, context);
}

void WOutputHelper::doneCurrent(QOpenGLContext *context)
{
    W_D(WOutputHelper);
    d->doneCurrent(context);
}

void WOutputHelper::resetState()
{
    W_D(WOutputHelper);
    d->setContentIsDirty(false);
    d->setRenderable(false);
    d->setNeedsFrame(false);
}

void WOutputHelper::update()
{
    W_D(WOutputHelper);
    d->update();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputhelper.cpp"

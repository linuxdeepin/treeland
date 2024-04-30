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
#include <wlr/types/wlr_output_layer.h>
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
        wlr_output_state_init(&state);

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
        }, Qt::QueuedConnection); // reset buffer on later, because it's rendering
    }

    ~WOutputHelperPrivate() {
        wlr_output_state_finish(&state);
    }

    inline QWOutput *qwoutput() const {
        return output->handle();
    }

    inline QWRenderer *renderer() const {
        return output->renderer();
    }

    inline QWlrootsOutputWindow *qpaWindow() const {
        return static_cast<QWlrootsOutputWindow*>(outputWindow->handle());
    }

    void setRenderable(bool newValue);
    void setContentIsDirty(bool newValue);
    void setNeedsFrame(bool newNeedsFrame);

    void on_frame();
    void on_damage();

    QWBuffer *acquireBuffer(wlr_swapchain **sc, int *bufferAge);

    inline void update() {
        setContentIsDirty(true);
    }

    W_DECLARE_PUBLIC(WOutputHelper)
    WOutput *output;
    wlr_output_state state;
    wlr_output_layer_state_array layersCache;
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

QWBuffer *WOutputHelperPrivate::acquireBuffer(wlr_swapchain **sc, int *bufferAge)
{
    // TODO: Use a new wlr_output_state in WOutputHelper
    bool ok = qwoutput()->configurePrimarySwapchain(&qwoutput()->handle()->pending, sc);
    if (!ok)
        return nullptr;
    QWBuffer *newBuffer = QWSwapchain::from(*sc)->acquire(bufferAge);
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

std::pair<QWBuffer *, QQuickRenderTarget> WOutputHelper::acquireRenderTarget(QQuickRenderControl *rc, int *bufferAge,
                                                                             wlr_swapchain **swapchain)
{
    W_D(WOutputHelper);

    QWBuffer *buffer = d->acquireBuffer(swapchain ? swapchain : &d->qwoutput()->handle()->swapchain, bufferAge);
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

void WOutputHelper::setBuffer(QWBuffer *buffer)
{
    W_D(WOutputHelper);
    wlr_output_state_set_buffer(&d->state, buffer->handle());
}

QWBuffer *WOutputHelper::buffer() const
{
    W_DC(WOutputHelper);
    return d->state.buffer ? QWBuffer::from(d->state.buffer) : nullptr;
}

void WOutputHelper::setScale(float scale)
{
    W_D(WOutputHelper);
    wlr_output_state_set_scale(&d->state, scale);
}

void WOutputHelper::setTransform(WOutput::Transform t)
{
    W_D(WOutputHelper);
    wlr_output_state_set_transform(&d->state, static_cast<wl_output_transform>(t));
}

void WOutputHelper::setDamage(const pixman_region32 *damage)
{
    W_D(WOutputHelper);
    wlr_output_state_set_damage(&d->state, damage);
}

const pixman_region32 *WOutputHelper::damage() const
{
    W_DC(WOutputHelper);
    return &d->state.damage;
}

void WOutputHelper::setLayers(const wlr_output_layer_state_array &layers)
{
    W_D(WOutputHelper);

    d->layersCache = layers;

    if (!layers.isEmpty()) {
        wlr_output_state_set_layers(&d->state, d->layersCache.data(), layers.length());
    } else {
        d->state.layers = nullptr;
        d->state.committed &= (~WLR_OUTPUT_STATE_LAYERS);
    }
}

bool WOutputHelper::commit()
{
    W_D(WOutputHelper);
    wlr_output_state state = d->state;
    wlr_output_state_init(&d->state);
    bool ok = d->qwoutput()->commitState(&state);
    wlr_output_state_finish(&state);

    return ok;
}

bool WOutputHelper::testCommit()
{
    W_D(WOutputHelper);
    return d->qwoutput()->testState(&d->state);
}

bool WOutputHelper::testCommit(QWBuffer *buffer, const wlr_output_layer_state_array &layers)
{
    W_D(WOutputHelper);
    wlr_output_state state = d->state;

    if (buffer)
        wlr_output_state_set_buffer(&state, buffer->handle());
    if (!layers.isEmpty())
        wlr_output_state_set_layers(&state, const_cast<wlr_output_layer_state*>(layers.data()), layers.length());

    bool ok = d->qwoutput()->testState(&state);
    if (state.committed & WLR_OUTPUT_STATE_BUFFER) {
        Q_ASSERT(buffer);
        buffer->unlock();
    }

    return ok;
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

void WOutputHelper::resetState(bool resetRenderable)
{
    W_D(WOutputHelper);
    d->setContentIsDirty(false);
    if (resetRenderable)
        d->setRenderable(false);
    d->setNeedsFrame(false);

    // reset output state
    if (d->state.committed & WLR_OUTPUT_STATE_BUFFER) {
        wlr_buffer_unlock(d->state.buffer);
        d->state.buffer = nullptr;
    }

    d->state.layers = nullptr;
    d->layersCache.clear();

    free(d->state.gamma_lut);
    d->state.gamma_lut = nullptr;
    pixman_region32_clear(&d->state.damage);
    d->state.committed = 0;
}

void WOutputHelper::update()
{
    W_D(WOutputHelper);
    d->update();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputhelper.cpp"

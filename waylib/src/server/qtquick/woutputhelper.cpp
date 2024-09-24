// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputhelper.h"
#include "wrenderhelper.h"
#include "woutput.h"
#include "platformplugin/types.h"
#include "private/wglobal_p.h"

#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwswapchain.h>
#include <qwbuffer.h>
#include <qwoutputlayer.h>

#include <platformplugin/qwlrootswindow.h>
#include <platformplugin/qwlrootsintegration.h>
#include <platformplugin/qwlrootscreen.h>

#include <QWindow>
#include <QQuickWindow>
#ifndef QT_NO_OPENGL
#include <QOpenGLContext>
#endif
#include <private/qquickwindow_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputHelperPrivate : public WObjectPrivate
{
public:
    WOutputHelperPrivate(WOutput *output, WOutputHelper *qq, bool r/* renderable */, bool c /*contentIsDirty*/, bool n/*needsFrame*/)
        : WObjectPrivate(qq)
        , output(output)
        , outputWindow(new QW::Window)
        , renderable(r)
        , contentIsDirty(c)
        , needsFrame(n)
    {
        wlr_output_state_init(&state);

        outputWindow->QObject::setParent(qq);
        outputWindow->setScreen(QWlrootsIntegration::instance()->getScreenFrom(output)->screen());
        outputWindow->create();

        output->safeConnect(&qw_output::notify_frame, qq, [this] {
            on_frame();
        });
        output->safeConnect(&qw_output::notify_needs_frame, qq, [this] {
            setNeedsFrame(true);
            qwoutput()->qw_output::schedule_frame();
        });
        output->safeConnect(&qw_output::notify_damage, qq, [this] {
            on_damage();
        });
        output->safeConnect(&WOutput::modeChanged, qq, [this] {
            if (renderHelper)
                renderHelper->setSize(this->output->size());
        }, Qt::QueuedConnection); // reset buffer on later, because it's rendering
    }

    ~WOutputHelperPrivate() {
        wlr_output_state_finish(&state);
    }

    inline qw_output *qwoutput() const {
        return output->handle();
    }

    inline qw_renderer *renderer() const {
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

    qw_buffer *acquireBuffer(wlr_swapchain **sc, int *bufferAge);

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

qw_buffer *WOutputHelperPrivate::acquireBuffer(wlr_swapchain **sc, int *bufferAge)
{
    // TODO: Use a new wlr_output_state in WOutputHelper
    bool ok = qwoutput()->configure_primary_swapchain(&qwoutput()->handle()->pending, sc);
    if (!ok)
        return nullptr;
    auto newBuffer = qw_swapchain::from(*sc)->acquire(bufferAge);
    return newBuffer ? qw_buffer::from(newBuffer) : nullptr;
}

WOutputHelper::WOutputHelper(WOutput *output, bool renderable, bool contentIsDirty, bool needsFrame, QObject *parent)
    : QObject(parent)
    , WObject(*new WOutputHelperPrivate(output, this, renderable, contentIsDirty, needsFrame))
{
}

WOutputHelper::WOutputHelper(WOutput *output, QObject *parent)
    : WOutputHelper(output, false, false, false, parent)
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

std::pair<qw_buffer *, QQuickRenderTarget> WOutputHelper::acquireRenderTarget(QQuickRenderControl *rc, int *bufferAge,
                                                                             wlr_swapchain **swapchain)
{
    W_D(WOutputHelper);

    qw_buffer *buffer = d->acquireBuffer(swapchain ? swapchain : &d->qwoutput()->handle()->swapchain, bufferAge);
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

std::pair<qw_buffer*, QQuickRenderTarget> WOutputHelper::lastRenderTarget()
{
    W_DC(WOutputHelper);
    if (!d->renderHelper)
        return {nullptr, {}};

    return d->renderHelper->lastRenderTarget();
}

void WOutputHelper::setBuffer(qw_buffer *buffer)
{
    W_D(WOutputHelper);
    wlr_output_state_set_buffer(&d->state, buffer->handle());
}

qw_buffer *WOutputHelper::buffer() const
{
    W_DC(WOutputHelper);
    return d->state.buffer ? qw_buffer::from(d->state.buffer) : nullptr;
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
    bool ok = d->qwoutput()->commit_state(&state);
    wlr_output_state_finish(&state);

    return ok;
}

bool WOutputHelper::testCommit()
{
    W_D(WOutputHelper);
    return d->qwoutput()->test_state(&d->state);
}

bool WOutputHelper::testCommit(qw_buffer *buffer, const wlr_output_layer_state_array &layers)
{
    W_D(WOutputHelper);
    wlr_output_state state = d->state;

    if (buffer)
        wlr_output_state_set_buffer(&state, buffer->handle());
    if (!layers.isEmpty())
        wlr_output_state_set_layers(&state, const_cast<wlr_output_layer_state*>(layers.data()), layers.length());

    bool ok = d->qwoutput()->test_state(&state);
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

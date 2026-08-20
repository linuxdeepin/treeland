// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputhelper.h"
#include "wayliblogging.h"
#include "wrenderhelper.h"
#include "woutput.h"
#include "platformplugin/types.h"
#include "private/wglobal_p.h"

#include <wlr_all.h>

#include <platformplugin/qwlrootswindow.h>
#include <platformplugin/qwlrootsintegration.h>
#include <platformplugin/qwlrootscreen.h>

#include <QWindow>
#include <QQuickWindow>
#ifndef QT_NO_OPENGL
#include <QOpenGLContext>
#endif
#include <private/qquickwindow_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputHelperPrivate : public WObjectPrivate
{
public:
    WOutputHelperPrivate(WOutput *output, WOutputHelper *qq, bool c /*contentIsDirty*/)
        : WObjectPrivate(qq)
        , output(output)
        , outputWindow(new QW::Window)
        , contentIsDirty(c)
    {
        wlr_output_state_init(&state);

        outputWindow->QObject::setParent(qq);
        outputWindow->setScreen(QWlrootsIntegration::instance()->getScreenFrom(output)->screen());
        outputWindow->create();

        // In wlroots, damage is triggered after a cursor move.
        // However, Waylib uses a custom cursor instead of having wlroots render it.
        // So, we don't need to listen to the damage signal."
        // (damage handled via wlr_output events.damage listener)
        QObject::connect(output, &WOutput::modeChanged, qq, [this] {
            if (renderHelper)
                renderHelper->setSize(this->output->size());
        }, Qt::QueuedConnection); // reset buffer on later, because it's rendering
    }

    ~WOutputHelperPrivate() {
        wlr_output_state_finish(&state);
    }

    inline wlr_output *qwoutput() const {
        return output->handle();
    }

    inline wlr_renderer *renderer() const {
        return output->renderer();
    }

    inline QWlrootsOutputWindow *qpaWindow() const {
        return static_cast<QWlrootsOutputWindow*>(outputWindow->handle());
    }

    void setContentIsDirty(bool newValue);

    wlr_buffer *acquireBuffer(wlr_swapchain **sc);

    inline void update() {
        setContentIsDirty(true);
        wlr_output_schedule_frame(qwoutput());
    }

    W_DECLARE_PUBLIC(WOutputHelper)
    WOutput *output;
    wlr_output_state state;
    wlr_output_layer_state_array layersCache;
    QWindow *outputWindow;
    WRenderHelper *renderHelper = nullptr;

    uint contentIsDirty:1;

    struct CommitJobEntry {
        std::function<void(bool, WOutputHelper::ExtraState)> jobWithState;
    };
    QList<CommitJobEntry> beforeCommitJobs;
    QList<CommitJobEntry> afterCommitJobs;

    // External state (mode/scale/transform/enabled) separate from internal (buffer/damage)
    WOutputHelper::ExtraState extraState;
};

void WOutputHelperPrivate::setContentIsDirty(bool newValue)
{
    if (contentIsDirty == newValue)
        return;
    contentIsDirty = newValue;
    Q_EMIT q_func()->contentIsDirtyChanged();
}

wlr_buffer *WOutputHelperPrivate::acquireBuffer(wlr_swapchain **sc)
{
    bool ok = wlr_output_configure_primary_swapchain(qwoutput(), &state, sc);
    if (!ok)
        return nullptr;
    return wlr_swapchain_acquire(*sc);
}

WOutputHelper::WOutputHelper(WOutput *output, bool contentIsDirty, QObject *parent)
    : QObject(parent)
    , WObject(*new WOutputHelperPrivate(output, this, contentIsDirty))
{
}

WOutputHelper::WOutputHelper(WOutput *output, QObject *parent)
    : WOutputHelper(output, true, parent)
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

WRenderHelper::RenderTarget WOutputHelper::acquireRenderTarget(QQuickRenderControl *rc,
                                                               wlr_swapchain **swapchain)
{
    W_D(WOutputHelper);

    wlr_buffer *buffer = d->acquireBuffer(swapchain ? swapchain : &d->qwoutput()->swapchain);
    if (!buffer)
        return {};

    if (!d->renderHelper) {
        d->renderHelper = new WRenderHelper(d->renderer(), this);
        d->renderHelper->setSize(d->output->size());
    }
    auto rt = d->renderHelper->acquireRenderTarget(rc, buffer);
    if (rt.isNull()) {
        wlr_buffer_unlock(buffer);
        return {};
    }

    return rt;
}

WRenderHelper::RenderTarget WOutputHelper::lastRenderTarget()
{
    W_DC(WOutputHelper);
    if (!d->renderHelper)
        return {};

    return d->renderHelper->lastRenderTarget();
}

void WOutputHelper::setBuffer(wlr_buffer *buffer)
{
    W_D(WOutputHelper);
    wlr_output_state_set_buffer(&d->state, buffer);
}

wlr_buffer *WOutputHelper::buffer() const
{
    W_DC(WOutputHelper);
    return d->state.buffer;
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

    // Execute before-commit jobs
    QList<WOutputHelperPrivate::CommitJobEntry> beforeJobs;
    beforeJobs.swap(d->beforeCommitJobs);
    for (const auto &entry : beforeJobs) {
        entry.jobWithState(true, d->extraState);
    }

    wlr_output_state state = d->state;
    wlr_output_state_init(&d->state);

    if (Q_UNLIKELY(d->extraState)) {
        // State-only commit: Use only extraState (no buffer/damage from internal state)
        // Finish the internal state as we're not using it
        wlr_output_state_finish(&state);
        wlr_output_state_copy(&state, d->extraState.get());
    }

    bool ok = wlr_output_commit_state(d->qwoutput(), &state);
    if (!ok) {
        qCCritical(lcWlOutputHelper, "commit failed on output %s", d->qwoutput()->name);
    }
    wlr_output_state_finish(&state);
    ExtraState committedExtraState = d->extraState;

    if (Q_UNLIKELY(d->extraState)) {
        d->extraState.reset();
    }

    QList<WOutputHelperPrivate::CommitJobEntry> afterJobs;
    afterJobs.swap(d->afterCommitJobs);
    for (const auto &entry : afterJobs) {
        entry.jobWithState(ok, committedExtraState);
    }
    return ok;
}

void WOutputHelper::scheduleCommitJob(CommitJobWithState job, CommitStage stage)
{
    W_D(WOutputHelper);
    WOutputHelperPrivate::CommitJobEntry entry;
    entry.jobWithState = job;

    if (stage == BeforeCommitStage) {
        d->beforeCommitJobs.append(entry);
    } else {
        d->afterCommitJobs.append(entry);
    }
}

bool WOutputHelper::setExtraState(ExtraState state)
{
    W_D(WOutputHelper);

    if (!state) {
        d->extraState.reset();
        return true;
    }

    // Only allow specific state flags for external configuration
    const uint32_t allowedFlags = WLR_OUTPUT_STATE_MODE |
                                  WLR_OUTPUT_STATE_SCALE |
                                  WLR_OUTPUT_STATE_TRANSFORM |
                                  WLR_OUTPUT_STATE_ENABLED |
                                  WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED |
                                  WLR_OUTPUT_STATE_COLOR_TRANSFORM;

    if (state->committed & ~allowedFlags) {
        qCWarning(lcWlOutputHelper) << "WOutputHelper::setExtraState: contains unsupported flags:"
                   << Qt::hex << (state->committed & ~allowedFlags);
        return false;
    }

    if ((state->committed & allowedFlags) == 0) {
        qCWarning(lcWlOutputHelper) << "WOutputHelper::setExtraState: no valid state changes";
        return false;
    }

    d->extraState = state;
    return true;
}

WOutputHelper::ExtraState WOutputHelper::extraState() const
{
    W_DC(WOutputHelper);
    return d->extraState;
}

bool WOutputHelper::testCommit()
{
    W_D(WOutputHelper);
    return wlr_output_test_state(d->qwoutput(), &d->state);
}

bool WOutputHelper::testCommit(wlr_buffer *buffer, const wlr_output_layer_state_array &layers)
{
    W_D(WOutputHelper);
    // Deep-copy: a shallow assign would share gamma_lut / other heap fields
    // with d->state, and set_buffer() could unlock a buffer still owned by
    // the helper's pending state.
    wlr_output_state state;
    wlr_output_state_init(&state);
    if (!wlr_output_state_copy(&state, &d->state)) {
        wlr_output_state_finish(&state);
        return false;
    }

    if (buffer)
        wlr_output_state_set_buffer(&state, buffer);
    if (!layers.isEmpty())
        wlr_output_state_set_layers(&state, const_cast<wlr_output_layer_state*>(layers.data()), layers.length());

    bool ok = wlr_output_test_state(d->qwoutput(), &state);
    // finish() unlocks any buffer we attached above.
    wlr_output_state_finish(&state);
    return ok;
}

bool WOutputHelper::contentIsDirty() const
{
    W_DC(WOutputHelper);
    return d->contentIsDirty;
}

bool WOutputHelper::needsFrame() const
{
    W_DC(WOutputHelper);
    return d->output->handle()->needs_frame;
}

bool WOutputHelper::framePending() const
{
    W_DC(WOutputHelper);
    return d->output->handle()->frame_pending;
}

void WOutputHelper::resetState()
{
    W_D(WOutputHelper);
    d->setContentIsDirty(false);

    // reset output state
    if (d->state.committed & WLR_OUTPUT_STATE_BUFFER) {
        wlr_buffer_unlock(d->state.buffer);
        d->state.buffer = nullptr;
    }

    d->state.layers = nullptr;
    d->layersCache.clear();

    wlr_color_transform_unref(d->state.color_transform);
    d->state.color_transform = nullptr;
    free(d->state.image_description);
    d->state.image_description = nullptr;
    pixman_region32_clear(&d->state.damage);
    d->state.committed = 0;
}

void WOutputHelper::update()
{
    W_D(WOutputHelper);
    d->update();
}

void WOutputHelper::scheduleFrame()
{
    W_D(WOutputHelper);
    wlr_output_schedule_frame(d->qwoutput());
}

bool WOutputHelper::willBeEnabled() const
{
    W_DC(WOutputHelper);
    // Check pending enabled state in priority order:
    // 1. extraState (external configuration from setExtraState)
    // 2. internal state (from setScale/setTransform)
    // 3. current state (from output->isEnabled())
    if (d->extraState && (d->extraState->committed & WLR_OUTPUT_STATE_ENABLED)) {
        return d->extraState->enabled;
    }
    if (d->state.committed & WLR_OUTPUT_STATE_ENABLED) {
        return d->state.enabled;
    }
    return d->output->isEnabled();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputhelper.cpp"

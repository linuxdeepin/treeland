// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wlr_fwd.h>

#include <QtCore/qglobal.h>
#include <QtCore/QElapsedTimer>

struct wlr_output_event_present;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputRenderWindow;

namespace WVulkanTrace {

enum class FrameStage {
    Idle,
    FrameBegin,
    Polish,
    BeginFrame,
    Sync,
    Recording,
    AfterRenderingJobs,
    ReleaseRenderBuffers,
    BeforeEndFrame,
    AfterEndFrame,
    OutputCommit,
    RenderEnd,
};

enum class SampleDisposition {
    Prepared,
    PrepareFailed,
    CurrentRenderTarget,
    InvalidProvider,
    DuplicateTexture,
};

// Sub-phases of a frame that are worth attributing separately. These overlap
// with FrameStage on purpose: FrameStage::Recording is the whole compositor
// draw, while these split it into its parts. They also nest: a layer pass runs
// its own BeginRender/QtDraw/TexPrep, so Work::Layer includes them; Work::Blitter
// is recorded from inside the Qt draw, so it is part of Work::QtDraw.
enum class Work {
    SceneWalk,
    BeginRender,
    QtDraw,
    TexPrep,
    // The barrier-batch flush inside TexPrep. A flush at NoPass makes QRhi
    // record (and reset) its deferred command list, so this is where Qt's
    // command recording can show up; kept separate from the prepare/finish
    // loops so the two cannot be confused in the trace.
    TexFlush,
    Blitter,
    Layer,
    // The texture-sync (producer fence) batch flush, which runs immediately
    // before endFrameChecked(). Split out from the frame stage so its cost can
    // be told apart from QRhi's own endFrame fence wait.
    SyncFlush,
    // Sub-parts of a barrier-batch flush. A flush at NoPass calls
    // QRhiCommandBuffer::beginExternal(), which makes QRhi record its whole
    // deferred command list right there. Splitting the flush is the only way
    // to tell Qt's command recording (FlushBegin) apart from our own
    // vkCmdPipelineBarrier (FlushBarrier) and the state reset (FlushEnd).
    FlushBegin,
    FlushBarrier,
    FlushEnd,
    // The two halves of the BeginFrame stage: QRhi's beginOffscreenFrame()
    // (command pool reset, deferred releases, command buffer begin) and the
    // texture-sync batch start (O(1) in wlroots). Splitting them tells a Qt
    // cost apart from a Waylib one.
    QtBeginFrame,
    SyncBatch,
    Count = SyncBatch,
};

struct CleanupToken {
    quint64 frameId = 0;
    quint64 sampleId = 0;
    quint64 clientBufferId = 0;
    quint64 sourceBufferId = 0;
    quintptr sourceBuffer = 0;
    quintptr window = 0;
    quintptr provider = 0;
    quintptr texture = 0;
    bool logLifecycle = false;
};

void activate(bool vulkanActive);
bool enabled();

void surfaceCommit(const void *surface, qint64 pid, quint64 surfaceSeq,
                   quint32 committedState, wlr_buffer *clientBuffer);
void providerBind(const void *provider, WOutputRenderWindow *window,
                  wlr_buffer *clientBuffer, wlr_texture *texture);
void providerReuse(const void *provider, WOutputRenderWindow *window,
                   wlr_buffer *clientBuffer, wlr_texture *texture);
void providerReject(const void *provider, WOutputRenderWindow *window,
                    wlr_buffer *clientBuffer, wlr_texture *texture,
                    const char *reason);
void providerDiscard(const void *provider, WOutputRenderWindow *window,
                     wlr_texture *texture,
                     const char *reason);
CleanupToken cleanupScheduled(const void *provider, WOutputRenderWindow *window,
                              wlr_buffer *clientBuffer, wlr_texture *texture);
void cleanupRunning(const CleanupToken &token);

void qtWrap(bool begin, wlr_texture *texture, quint64 image,
            quint32 vkFormat, int rhiFormat, const char *bridgeClass,
            const char *layout);

// RAII timer attributing a sub-phase of the current frame. No-op (no timer,
// no lock) unless VKTRACE is active, so the instrumentation is free in normal
// operation.
class ScopedWork
{
public:
    ScopedWork(WOutputRenderWindow *window, Work work);
    ~ScopedWork();

    ScopedWork(const ScopedWork &) = delete;
    ScopedWork &operator=(const ScopedWork &) = delete;

private:
    WOutputRenderWindow *m_window = nullptr;
    Work m_work = Work::SceneWalk;
    bool m_active = false;
    QElapsedTimer m_timer;
};

void beginFrame(WOutputRenderWindow *window);
void setFrameStage(WOutputRenderWindow *window, FrameStage stage);
void endFrame(WOutputRenderWindow *window, qsizetype committedOutputCount);
void windowDestroyed(WOutputRenderWindow *window);

void beginPass(WOutputRenderWindow *window, const void *renderer,
               wlr_buffer *targetBuffer, const char *purpose,
               int sourceIndex, qsizetype providerCount);
// A provider wrapped a texture while no render pass was active: the dynamic
// acquire is impossible here and the next static prepass covers it.
void dynamicPrepareSkipped(WOutputRenderWindow *window, wlr_texture *texture);
// Number of image barriers a flushed batch contained. Bounded by twice the
// number of sampled textures, so a large value means barriers are
// accumulating across flushes.
void textureBarrierBatchCount(WOutputRenderWindow *window, int count);
void sampleDisposition(WOutputRenderWindow *window, const void *provider,
                       wlr_texture *texture,
                       SampleDisposition disposition, qint64 elapsedUsec = 0);
void sampleDynamicDisposition(WOutputRenderWindow *window,
                              wlr_texture *texture,
                              SampleDisposition disposition,
                              qint64 elapsedUsec = 0);
void sampleFinished(WOutputRenderWindow *window,
                    wlr_texture *texture, bool ok,
                    qint64 elapsedUsec);
void surfaceFootprint(WOutputRenderWindow *window, const void *provider,
                      wlr_texture *texture, const void *surface);
void endPass(WOutputRenderWindow *window, bool ok);

void outputCommitState(wlr_output *output, quint32 committedState);
void outputCommitted(WOutputRenderWindow *window,
                     wlr_output *output,
                     quint32 sequenceBefore, bool ok);
void outputPresented(wlr_output *output,
                     const wlr_output_event_present *event);
void outputDestroyed(wlr_output *output);

} // namespace WVulkanTrace

WAYLIB_SERVER_END_NAMESPACE

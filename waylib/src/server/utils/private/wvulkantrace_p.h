// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QtCore/qglobal.h>

struct wlr_output_event_present;

QW_BEGIN_NAMESPACE
class qw_buffer;
class qw_output;
class qw_texture;
QW_END_NAMESPACE

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
                   quint32 committedState, QW_NAMESPACE::qw_buffer *clientBuffer);
void providerBind(const void *provider, WOutputRenderWindow *window,
                  QW_NAMESPACE::qw_buffer *clientBuffer,
                  QW_NAMESPACE::qw_texture *texture);
void providerReuse(const void *provider, WOutputRenderWindow *window,
                   QW_NAMESPACE::qw_buffer *clientBuffer,
                   QW_NAMESPACE::qw_texture *texture);
void providerReject(const void *provider, WOutputRenderWindow *window,
                    QW_NAMESPACE::qw_buffer *clientBuffer,
                    QW_NAMESPACE::qw_texture *texture,
                    const char *reason);
void providerDiscard(const void *provider, WOutputRenderWindow *window,
                     QW_NAMESPACE::qw_texture *texture,
                     const char *reason);
CleanupToken cleanupScheduled(const void *provider, WOutputRenderWindow *window,
                              QW_NAMESPACE::qw_buffer *clientBuffer,
                              QW_NAMESPACE::qw_texture *texture);
void cleanupRunning(const CleanupToken &token);

void qtWrap(bool begin, QW_NAMESPACE::qw_texture *texture, quint64 image,
            quint32 vkFormat, int rhiFormat, const char *bridgeClass,
            const char *layout);

void beginFrame(WOutputRenderWindow *window);
void setFrameStage(WOutputRenderWindow *window, FrameStage stage);
void endFrame(WOutputRenderWindow *window, qsizetype committedOutputCount);
void windowDestroyed(WOutputRenderWindow *window);

void beginPass(WOutputRenderWindow *window, const void *renderer,
               QW_NAMESPACE::qw_buffer *targetBuffer, const char *purpose,
               int sourceIndex, qsizetype providerCount);
void sampleDisposition(WOutputRenderWindow *window, const void *provider,
                       QW_NAMESPACE::qw_texture *texture,
                       SampleDisposition disposition, qint64 elapsedUsec = 0);
void sampleFinished(WOutputRenderWindow *window,
                    QW_NAMESPACE::qw_texture *texture, bool ok,
                    qint64 elapsedUsec);
void surfaceFootprint(WOutputRenderWindow *window, const void *provider,
                      QW_NAMESPACE::qw_texture *texture, const void *surface);
void endPass(WOutputRenderWindow *window, bool ok);

void outputCommitState(QW_NAMESPACE::qw_output *output, quint32 committedState);
void outputCommitted(WOutputRenderWindow *window,
                     QW_NAMESPACE::qw_output *output,
                     quint32 sequenceBefore, bool ok);
void outputPresented(QW_NAMESPACE::qw_output *output,
                     const wlr_output_event_present *event);
void outputDestroyed(QW_NAMESPACE::qw_output *output);

} // namespace WVulkanTrace

WAYLIB_SERVER_END_NAMESPACE

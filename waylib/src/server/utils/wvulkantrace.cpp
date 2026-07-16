// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "private/wvulkantrace_p.h"

#include "wayliblogging.h"
#include "woutputrenderwindow.h"

#include <qwbuffer.h>
#include <qwcompositor.h>
#include <qwoutput.h>
#include <qwtexture.h>

#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <drm_fourcc.h>
#include <xf86drm.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdlib>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

namespace WVulkanTrace {
namespace {

constexpr qsizetype MaxTrackedOutputCommits = 32;

struct BufferRecord {
    quint64 sourceBufferId = 0;
    quint64 generation = 0;
    quint64 releaseSequence = 0;
    quint64 lastFrameId = 0;
    quint64 lastDrawFrameId = 0;
    quint64 lastSampleId = 0;
    quintptr lastWindow = 0;
    quintptr lastDrawWindow = 0;
    quintptr lastSurface = 0;
    qint64 pid = 0;
    quint64 surfaceSequence = 0;
    int width = 0;
    int height = 0;
    quint32 format = 0;
    quint64 modifier = 0;
    int planeCount = 0;
    QByteArray bufferType = QByteArrayLiteral("other");
    bool signatureLogged = false;
    bool qtWrapLogged = false;
    bool cleanupLogged = false;
    std::array<quint32, WLR_DMABUF_MAX_PLANES> offsets {};
    std::array<quint32, WLR_DMABUF_MAX_PLANES> strides {};
};

struct ClientBufferRecord {
    quint64 clientBufferId = 0;
    quint64 sourceBufferId = 0;
    quintptr sourceBuffer = 0;
};

struct ProviderRecord {
    quint64 sampleId = 0;
    quint64 clientBufferId = 0;
    quint64 sourceBufferId = 0;
    quintptr sourceBuffer = 0;
    quintptr window = 0;
    quintptr provider = 0;
    quintptr texture = 0;
    bool logQtWrap = false;
    bool reuseLogged = false;
};

struct PassRecord {
    quint64 passId = 0;
    quintptr renderer = 0;
    quintptr targetBuffer = 0;
    QByteArray purpose;
    int sourceIndex = -1;
    qsizetype providerCount = 0;
    QSet<quintptr> preparedTextures;
    QHash<quintptr, SampleDisposition> providerDispositions;
    int prepareCount = 0;
    int prepareFailureCount = 0;
    int finishCount = 0;
    int finishFailureCount = 0;
    int footprintCount = 0;
    int missingProviderCount = 0;
    int drawAfterPrepareFailureCount = 0;
    int currentTargetFeedbackCount = 0;
    int invalidProviderDrawCount = 0;
};

struct WindowRecord {
    quint64 frameId = 0;
    FrameStage stage = FrameStage::Idle;
    QElapsedTimer frameTimer;
    QElapsedTimer endFrameTimer;
    qint64 endFrameUsec = 0;
    QVector<PassRecord> passStack;
    QSet<quint64> bufferIds;
    int passCount = 0;
    int prepareCount = 0;
    int prepareFailureCount = 0;
    int finishCount = 0;
    int finishFailureCount = 0;
    int footprintCount = 0;
    int missingPrepareCount = 0;
    int releaseBeforeEndFrameCandidateCount = 0;
};

struct OutputCommitRecord {
    quint64 frameId = 0;
    quint32 committedState = 0;
};

QMutex s_mutex;
QHash<quintptr, BufferRecord> s_sourceBuffers;
QHash<quintptr, ClientBufferRecord> s_clientBuffers;
QHash<quintptr, ProviderRecord> s_providers;
QHash<quintptr, ProviderRecord> s_textures;
QHash<quintptr, WindowRecord> s_windows;
QHash<quintptr, QMap<quint32, OutputCommitRecord>> s_outputCommits;
QHash<quintptr, QMap<quint32, quint32>> s_outputCommitStates;

std::atomic_bool s_active { false };
std::atomic<quint64> s_nextFrameId { 1 };
std::atomic<quint64> s_nextPassId { 1 };
std::atomic<quint64> s_nextBufferId { 1 };
std::atomic<quint64> s_nextSampleId { 1 };

QString pointerName(quintptr value)
{
    return QStringLiteral("0x%1").arg(value, QT_POINTER_SIZE * 2, 16, QLatin1Char('0'));
}

QString hex32(quint32 value)
{
    return QStringLiteral("0x%1").arg(value, 8, 16, QLatin1Char('0'));
}

QString hex64(quint64 value)
{
    return QStringLiteral("0x%1").arg(value, 16, 16, QLatin1Char('0'));
}

const char *stageName(FrameStage stage)
{
    switch (stage) {
    case FrameStage::Idle:
        return "idle";
    case FrameStage::FrameBegin:
        return "frame-begin";
    case FrameStage::Polish:
        return "polish";
    case FrameStage::BeginFrame:
        return "begin-frame";
    case FrameStage::Sync:
        return "sync";
    case FrameStage::Recording:
        return "recording";
    case FrameStage::AfterRenderingJobs:
        return "after-rendering-jobs";
    case FrameStage::ReleaseRenderBuffers:
        return "release-render-buffers";
    case FrameStage::BeforeEndFrame:
        return "before-end-frame";
    case FrameStage::AfterEndFrame:
        return "after-end-frame";
    case FrameStage::OutputCommit:
        return "output-commit";
    case FrameStage::RenderEnd:
        return "render-end";
    }
    return "unknown";
}

const char *dispositionName(SampleDisposition disposition)
{
    switch (disposition) {
    case SampleDisposition::Prepared:
        return "prepared";
    case SampleDisposition::PrepareFailed:
        return "prepare-failed";
    case SampleDisposition::CurrentRenderTarget:
        return "current-render-target";
    case SampleDisposition::InvalidProvider:
        return "invalid-provider";
    case SampleDisposition::DuplicateTexture:
        return "duplicate-texture";
    }
    return "unknown";
}

qw_buffer *sourceBufferFor(qw_buffer *clientBuffer, bool createWrapper)
{
    if (!clientBuffer || !clientBuffer->handle())
        return nullptr;

    auto *client = qw_client_buffer::get(*clientBuffer);
    if (client) {
        if (auto *source = client->source())
            return source;
        // The producer may already have destroyed its wl_buffer. The locked
        // client buffer remains a useful stable identity for lifetime tracing.
        return clientBuffer;
    }

    Q_UNUSED(createWrapper);
    return clientBuffer;
}

void removeClientBuffer(quintptr key)
{
    QMutexLocker locker(&s_mutex);
    s_clientBuffers.remove(key);
}

void sourceReleased(quintptr sourceKey, qw_buffer *sourceBuffer)
{
    QString message;
    bool candidate = false;
    {
        QMutexLocker locker(&s_mutex);
        auto sourceIt = s_sourceBuffers.find(sourceKey);
        if (sourceIt == s_sourceBuffers.end())
            return;

        auto &source = sourceIt.value();
        ++source.releaseSequence;

        bool dropped = true;
        size_t lockCount = 0;
        if (sourceBuffer && sourceBuffer->handle()) {
            dropped = sourceBuffer->handle()->dropped;
            lockCount = sourceBuffer->handle()->n_locks;
        }

        FrameStage stage = FrameStage::Idle;
        auto windowIt = s_windows.find(source.lastDrawWindow);
        if (windowIt != s_windows.end()) {
            auto &window = windowIt.value();
            stage = window.stage;
            candidate = !dropped
                        && source.lastDrawFrameId != 0
                        && source.lastDrawFrameId == window.frameId
                        && stage != FrameStage::AfterEndFrame
                        && stage != FrameStage::OutputCommit
                        && stage != FrameStage::RenderEnd
                        && stage != FrameStage::Idle;
            if (candidate)
                ++window.releaseBeforeEndFrameCandidateCount;
        }

        message = QStringLiteral("VKTRACE event=source-release-emitted frame=%1 stage=%2 sourceBufferId=%3 sourceBuffer=%4 releaseSeq=%5 sampleId=%6 lastDrawFrame=%7 dropped=%8 locks=%9 releaseBeforeEndFrameCandidate=%10")
                      .arg(source.lastFrameId)
                      .arg(QLatin1StringView(stageName(stage)))
                      .arg(source.sourceBufferId)
                      .arg(pointerName(sourceKey))
                      .arg(source.releaseSequence)
                      .arg(source.lastSampleId)
                      .arg(source.lastDrawFrameId)
                      .arg(dropped)
                      .arg(lockCount)
                      .arg(candidate);
    }

    if (candidate)
        qCWarning(lcWlQtQuickTexture).noquote() << message;
    else
        qCDebug(lcWlQtQuickTexture).noquote() << message;
}

void sourceDestroyed(quintptr sourceKey)
{
    QString message;
    {
        QMutexLocker locker(&s_mutex);
        auto sourceIt = s_sourceBuffers.find(sourceKey);
        if (sourceIt == s_sourceBuffers.end())
            return;
        message = QStringLiteral("VKTRACE event=source-destroy sourceBufferId=%1 sourceBuffer=%2 releaseSeq=%3")
                      .arg(sourceIt->sourceBufferId)
                      .arg(pointerName(sourceKey))
                      .arg(sourceIt->releaseSequence);
        s_sourceBuffers.erase(sourceIt);
    }
    qCDebug(lcWlQtQuickTexture).noquote() << message;
}

bool registerBuffers(qw_buffer *clientBuffer, bool createSourceWrapper,
                     quint64 *clientBufferId, quint64 *sourceBufferId,
                     quintptr *sourceKey)
{
    if (clientBufferId)
        *clientBufferId = 0;
    if (sourceBufferId)
        *sourceBufferId = 0;
    if (sourceKey)
        *sourceKey = 0;
    if (!clientBuffer || !clientBuffer->handle())
        return false;

    const quintptr clientKey = reinterpret_cast<quintptr>(clientBuffer->handle());
    if (!createSourceWrapper) {
        QMutexLocker locker(&s_mutex);
        const auto client = s_clientBuffers.value(clientKey);
        if (!client.clientBufferId || !client.sourceBufferId)
            return false;
        if (clientBufferId)
            *clientBufferId = client.clientBufferId;
        if (sourceBufferId)
            *sourceBufferId = client.sourceBufferId;
        if (sourceKey)
            *sourceKey = client.sourceBuffer;
        return true;
    }

    auto *source = sourceBufferFor(clientBuffer, true);
    if (!source || !source->handle())
        return false;

    wlr_dmabuf_attributes dmabuf {};
    wlr_shm_attributes shm {};
    const bool isDmabuf = source->get_dmabuf(&dmabuf);
    const bool isShm = !isDmabuf && source->get_shm(&shm);

    const quintptr localSourceKey = reinterpret_cast<quintptr>(source->handle());
    bool connectSourceSignals = false;
    quint64 localSourceId = 0;
    {
        QMutexLocker locker(&s_mutex);
        auto it = s_sourceBuffers.find(localSourceKey);
        if (it == s_sourceBuffers.end()) {
            BufferRecord record;
            record.sourceBufferId = s_nextBufferId.fetch_add(1, std::memory_order_relaxed);
            if (isDmabuf) {
                record.bufferType = QByteArrayLiteral("dmabuf");
                record.width = dmabuf.width;
                record.height = dmabuf.height;
                record.format = dmabuf.format;
                record.modifier = dmabuf.modifier;
                record.planeCount = dmabuf.n_planes;
                for (int i = 0; i < dmabuf.n_planes && i < WLR_DMABUF_MAX_PLANES; ++i) {
                    record.offsets[i] = dmabuf.offset[i];
                    record.strides[i] = dmabuf.stride[i];
                }
            } else if (isShm) {
                record.bufferType = QByteArrayLiteral("shm");
                record.width = shm.width;
                record.height = shm.height;
                record.format = shm.format;
                record.modifier = DRM_FORMAT_MOD_INVALID;
                record.planeCount = 1;
                record.offsets[0] = quint32(shm.offset);
                record.strides[0] = quint32(shm.stride);
            } else {
                record.width = source->handle()->width;
                record.height = source->handle()->height;
            }
            localSourceId = record.sourceBufferId;
            s_sourceBuffers.insert(localSourceKey, record);
            connectSourceSignals = true;
        } else {
            localSourceId = it->sourceBufferId;
        }
    }

    if (connectSourceSignals) {
        QObject::connect(source, &qw_buffer::notify_release, source,
                         [localSourceKey, source] { sourceReleased(localSourceKey, source); },
                         Qt::DirectConnection);
        QObject::connect(source, &qw_buffer::before_destroy, source,
                         [localSourceKey] { sourceDestroyed(localSourceKey); },
                         Qt::DirectConnection);
    }

    bool connectClientDestroy = false;
    quint64 localClientId = 0;
    {
        QMutexLocker locker(&s_mutex);
        auto it = s_clientBuffers.find(clientKey);
        if (it == s_clientBuffers.end()) {
            ClientBufferRecord record;
            record.clientBufferId = s_nextBufferId.fetch_add(1, std::memory_order_relaxed);
            record.sourceBufferId = localSourceId;
            record.sourceBuffer = localSourceKey;
            localClientId = record.clientBufferId;
            s_clientBuffers.insert(clientKey, record);
            connectClientDestroy = true;
        } else {
            localClientId = it->clientBufferId;
            it->sourceBufferId = localSourceId;
            it->sourceBuffer = localSourceKey;
        }
    }
    if (connectClientDestroy) {
        QObject::connect(clientBuffer, &qw_buffer::before_destroy, clientBuffer,
                         [clientKey] { removeClientBuffer(clientKey); },
                         Qt::DirectConnection);
    }

    if (clientBufferId)
        *clientBufferId = localClientId;
    if (sourceBufferId)
        *sourceBufferId = localSourceId;
    if (sourceKey)
        *sourceKey = localSourceKey;
    return true;
}

QString formatName(quint32 format)
{
    char *name = drmGetFormatName(format);
    if (!name)
        return QStringLiteral("unknown");
    const QString result = QString::fromLatin1(name);
    free(name);
    return result;
}

QString modifierName(quint64 modifier)
{
    char *name = drmGetFormatModifierName(modifier);
    if (!name)
        return QStringLiteral("unknown");
    const QString result = QString::fromLatin1(name);
    free(name);
    return result;
}

ProviderRecord providerRecordForTexture(quintptr texture)
{
    QMutexLocker locker(&s_mutex);
    return s_textures.value(texture);
}

} // namespace

void activate(bool vulkanActive)
{
    static const bool requested = qEnvironmentVariableIntValue("WAYLIB_VULKAN_TRACE") != 0;
    if (!requested || !vulkanActive)
        return;

    bool expected = false;
    if (!s_active.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        return;

    qCDebug(lcWlRenderHelper).noquote()
        << QStringLiteral("VKTRACE event=trace-start pid=%1")
               .arg(QCoreApplication::applicationPid());
}

bool enabled()
{
    return s_active.load(std::memory_order_relaxed);
}

void surfaceCommit(const void *surface, qint64 pid, quint64 surfaceSeq,
                   quint32 committedState, qw_buffer *clientBuffer)
{
    if (!enabled() || !(committedState & WLR_SURFACE_STATE_BUFFER))
        return;

    if (!clientBuffer) {
        qCDebug(lcWlSurface).noquote()
            << QStringLiteral("VKTRACE event=surface-commit surface=%1 pid=%2 surfaceSeq=%3 committed=%4 generation=0 clientBufferId=0 clientBuffer=0x0000000000000000 sourceBufferId=0 sourceBuffer=0x0000000000000000 texture=0x0000000000000000 bufferType=none")
                   .arg(pointerName(reinterpret_cast<quintptr>(surface)))
                   .arg(pid)
                   .arg(surfaceSeq)
                   .arg(hex32(committedState));
        return;
    }

    quint64 clientId = 0;
    quint64 sourceId = 0;
    quintptr sourceKey = 0;
    if (!registerBuffers(clientBuffer, true, &clientId, &sourceId, &sourceKey))
        return;

    quintptr texture = 0;
    if (auto *client = qw_client_buffer::get(*clientBuffer))
        texture = reinterpret_cast<quintptr>(client->texture());

    quint64 generation = 0;
    QByteArray bufferType;
    {
        QMutexLocker locker(&s_mutex);
        auto &source = s_sourceBuffers[sourceKey];
        generation = ++source.generation;
        source.lastSurface = reinterpret_cast<quintptr>(surface);
        source.pid = pid;
        source.surfaceSequence = surfaceSeq;
        bufferType = source.bufferType;
    }

    qCDebug(lcWlSurface).noquote()
        << QStringLiteral("VKTRACE event=surface-commit surface=%1 pid=%2 surfaceSeq=%3 committed=%4 generation=%5 clientBufferId=%6 clientBuffer=%7 sourceBufferId=%8 sourceBuffer=%9 texture=%10 bufferType=%11")
               .arg(pointerName(reinterpret_cast<quintptr>(surface)))
               .arg(pid)
               .arg(surfaceSeq)
               .arg(hex32(committedState))
               .arg(generation)
               .arg(clientId)
               .arg(pointerName(reinterpret_cast<quintptr>(clientBuffer->handle())))
               .arg(sourceId)
               .arg(pointerName(sourceKey))
               .arg(pointerName(texture))
               .arg(QString::fromLatin1(bufferType));
}

void providerBind(const void *provider, WOutputRenderWindow *window,
                  qw_buffer *clientBuffer, qw_texture *texture)
{
    if (!enabled() || !provider || !window || !clientBuffer || !texture)
        return;

    quint64 clientId = 0;
    quint64 sourceId = 0;
    quintptr sourceKey = 0;
    if (!registerBuffers(clientBuffer, false, &clientId, &sourceId, &sourceKey))
        return;

    const auto *ownerClientBuffer = qw_client_buffer::get(*clientBuffer);
    const char *ownerStatus = !ownerClientBuffer
        ? "provider-owned"
        : (qw_texture::from(ownerClientBuffer->texture()) == texture
               ? "matched"
               : "mismatch");

    ProviderRecord providerRecord;
    BufferRecord sourceRecord;
    FrameStage stage = FrameStage::Idle;
    quint64 frameId = 0;
    {
        QMutexLocker locker(&s_mutex);
        auto sourceIt = s_sourceBuffers.find(sourceKey);
        if (sourceIt == s_sourceBuffers.end())
            return;

        providerRecord.sampleId = s_nextSampleId.fetch_add(1, std::memory_order_relaxed);
        providerRecord.clientBufferId = clientId;
        providerRecord.sourceBufferId = sourceId;
        providerRecord.sourceBuffer = sourceKey;
        providerRecord.window = reinterpret_cast<quintptr>(window);
        providerRecord.provider = reinterpret_cast<quintptr>(provider);
        providerRecord.texture = reinterpret_cast<quintptr>(texture);
        providerRecord.logQtWrap = true;
        sourceIt->qtWrapLogged = true;
        sourceIt->signatureLogged = true;
        s_providers.insert(providerRecord.provider, providerRecord);
        s_textures.insert(providerRecord.texture, providerRecord);

        sourceIt->lastSampleId = providerRecord.sampleId;
        sourceIt->lastWindow = providerRecord.window;
        sourceRecord = sourceIt.value();

        auto windowIt = s_windows.find(providerRecord.window);
        if (windowIt != s_windows.end()) {
            stage = windowIt->stage;
            frameId = windowIt->frameId;
            windowIt->bufferIds.insert(sourceId);
            sourceIt->lastFrameId = frameId;
        }
    }

    QStringList planes;
    for (int i = 0; i < sourceRecord.planeCount && i < WLR_DMABUF_MAX_PLANES; ++i) {
        planes.append(QStringLiteral("%1:%2/%3")
                          .arg(i)
                          .arg(sourceRecord.offsets[i])
                          .arg(sourceRecord.strides[i]));
    }

    qCDebug(lcWlQtQuickTexture).noquote()
        << QStringLiteral("VKTRACE event=provider-bind action=wrap owner=%1 frame=%2 stage=%3 sampleId=%4 provider=%5 clientBufferId=%6 sourceBufferId=%7 generation=%8 texture=%9 size=%10x%11 bufferType=%12 fourcc=%13 fourccHex=%14 modifier=%15 modifierHex=%16 planeCount=%17 planes=%18")
               .arg(QLatin1StringView(ownerStatus))
               .arg(frameId)
               .arg(QLatin1StringView(stageName(stage)))
               .arg(providerRecord.sampleId)
               .arg(pointerName(providerRecord.provider))
               .arg(clientId)
               .arg(sourceId)
               .arg(sourceRecord.generation)
               .arg(pointerName(providerRecord.texture))
               .arg(sourceRecord.width)
               .arg(sourceRecord.height)
               .arg(QString::fromLatin1(sourceRecord.bufferType))
               .arg(formatName(sourceRecord.format))
               .arg(hex32(sourceRecord.format))
               .arg(modifierName(sourceRecord.modifier))
               .arg(hex64(sourceRecord.modifier))
               .arg(sourceRecord.planeCount)
               .arg(planes.join(QLatin1Char(',')));
}

void providerReuse(const void *provider, WOutputRenderWindow *window,
                   qw_buffer *clientBuffer, qw_texture *texture)
{
    if (!enabled() || !provider || !window || !clientBuffer || !texture)
        return;

    const quintptr providerKey = reinterpret_cast<quintptr>(provider);
    FrameStage stage = FrameStage::Idle;
    quint64 frameId = 0;
    ProviderRecord record;
    {
        QMutexLocker locker(&s_mutex);
        auto it = s_providers.find(providerKey);
        if (it == s_providers.end()
            || it->texture != reinterpret_cast<quintptr>(texture)
            || it->reuseLogged) {
            return;
        }
        it->reuseLogged = true;
        record = it.value();

        auto windowIt = s_windows.find(reinterpret_cast<quintptr>(window));
        if (windowIt != s_windows.end()) {
            frameId = windowIt->frameId;
            stage = windowIt->stage;
        }
    }

    qCDebug(lcWlQtQuickTexture).noquote()
        << QStringLiteral("VKTRACE event=provider-bind action=reuse owner=matched frame=%1 stage=%2 sampleId=%3 provider=%4 clientBufferId=%5 sourceBufferId=%6 texture=%7")
               .arg(frameId)
               .arg(QLatin1StringView(stageName(stage)))
               .arg(record.sampleId)
               .arg(pointerName(providerKey))
               .arg(record.clientBufferId)
               .arg(record.sourceBufferId)
               .arg(pointerName(reinterpret_cast<quintptr>(texture)));
}

void providerReject(const void *provider, WOutputRenderWindow *window,
                    qw_buffer *clientBuffer, qw_texture *texture,
                    const char *reason)
{
    if (!enabled() || !provider || !window || !texture)
        return;

    FrameStage stage = FrameStage::Idle;
    quint64 frameId = 0;
    {
        QMutexLocker locker(&s_mutex);
        auto windowIt = s_windows.find(reinterpret_cast<quintptr>(window));
        if (windowIt != s_windows.end()) {
            frameId = windowIt->frameId;
            stage = windowIt->stage;
        }
    }

    qCDebug(lcWlQtQuickTexture).noquote()
        << QStringLiteral("VKTRACE event=provider-bind action=reject owner=%1 frame=%2 stage=%3 provider=%4 clientBuffer=%5 texture=%6")
               .arg(QLatin1StringView(reason ? reason : "unknown"))
               .arg(frameId)
               .arg(QLatin1StringView(stageName(stage)))
               .arg(pointerName(reinterpret_cast<quintptr>(provider)))
               .arg(pointerName(clientBuffer
                                    ? reinterpret_cast<quintptr>(clientBuffer->handle())
                                    : 0))
               .arg(pointerName(reinterpret_cast<quintptr>(texture)));
}

void providerDiscard(const void *provider, WOutputRenderWindow *window,
                     qw_texture *texture, const char *reason)
{
    if (!enabled() || !provider || !window || !texture)
        return;

    const quintptr providerKey = reinterpret_cast<quintptr>(provider);
    const quintptr textureKey = reinterpret_cast<quintptr>(texture);
    ProviderRecord discarded;
    FrameStage stage = FrameStage::Idle;
    quint64 frameId = 0;
    {
        QMutexLocker locker(&s_mutex);
        auto textureIt = s_textures.find(textureKey);
        if (textureIt != s_textures.end()
            && textureIt->provider == providerKey) {
            discarded = textureIt.value();
            s_textures.erase(textureIt);
        }

        auto providerIt = s_providers.find(providerKey);
        if (discarded.sampleId
            && providerIt != s_providers.end()
            && providerIt->sampleId == discarded.sampleId) {
            s_providers.erase(providerIt);

            // A transactional replacement can fail after providerBind() has
            // made the candidate visible to tracing. Restore the newest older
            // sample so later cleanup and draw records still describe the
            // texture that the provider actually kept.
            ProviderRecord previous;
            for (auto it = s_textures.cbegin(); it != s_textures.cend(); ++it) {
                if (it->provider == providerKey
                    && it->sampleId > previous.sampleId) {
                    previous = it.value();
                }
            }
            if (previous.sampleId)
                s_providers.insert(providerKey, previous);
        }

        auto windowIt = s_windows.find(reinterpret_cast<quintptr>(window));
        if (windowIt != s_windows.end()) {
            frameId = windowIt->frameId;
            stage = windowIt->stage;
        }
    }

    qCDebug(lcWlQtQuickTexture).noquote()
        << QStringLiteral("VKTRACE event=provider-bind action=discard frame=%1 stage=%2 sampleId=%3 provider=%4 clientBufferId=%5 sourceBufferId=%6 texture=%7 reason=%8")
               .arg(frameId)
               .arg(QLatin1StringView(stageName(stage)))
               .arg(discarded.sampleId)
               .arg(pointerName(providerKey))
               .arg(discarded.clientBufferId)
               .arg(discarded.sourceBufferId)
               .arg(pointerName(textureKey))
               .arg(QLatin1StringView(reason ? reason : "unknown"));
}

CleanupToken cleanupScheduled(const void *provider, WOutputRenderWindow *window,
                              qw_buffer *clientBuffer, qw_texture *texture)
{
    CleanupToken token;
    if (!enabled())
        return token;

    const quintptr providerKey = reinterpret_cast<quintptr>(provider);
    FrameStage stage = FrameStage::Idle;
    {
        QMutexLocker locker(&s_mutex);
        const auto textureRecord = s_textures.value(
            reinterpret_cast<quintptr>(texture));
        const auto providerRecord = textureRecord.provider == providerKey
            ? textureRecord
            : s_providers.value(providerKey);
        token.sampleId = providerRecord.sampleId;
        token.clientBufferId = providerRecord.clientBufferId;
        token.sourceBufferId = providerRecord.sourceBufferId;
        token.sourceBuffer = providerRecord.sourceBuffer;
        token.window = reinterpret_cast<quintptr>(window);
        token.provider = providerKey;
        token.texture = reinterpret_cast<quintptr>(texture);
        auto windowIt = s_windows.find(token.window);
        if (windowIt != s_windows.end()) {
            token.frameId = windowIt->frameId;
            stage = windowIt->stage;
        }
    }

    if (!token.sourceBufferId && clientBuffer) {
        QMutexLocker locker(&s_mutex);
        const auto client = s_clientBuffers.value(reinterpret_cast<quintptr>(clientBuffer->handle()));
        token.clientBufferId = client.clientBufferId;
        token.sourceBufferId = client.sourceBufferId;
        token.sourceBuffer = client.sourceBuffer;
    }

    if (!token.sourceBufferId)
        return token;

    {
        QMutexLocker locker(&s_mutex);
        auto sourceIt = s_sourceBuffers.find(token.sourceBuffer);
        if (sourceIt != s_sourceBuffers.end()
            && sourceIt->sourceBufferId == token.sourceBufferId
            && !sourceIt->cleanupLogged) {
            sourceIt->cleanupLogged = true;
            token.logLifecycle = true;
        }
    }

    if (!token.logLifecycle)
        return token;

    qCDebug(lcWlQtQuickTexture).noquote()
        << QStringLiteral("VKTRACE event=cleanup-scheduled frame=%1 stage=%2 sampleId=%3 provider=%4 clientBufferId=%5 sourceBufferId=%6 texture=%7 cleanupStage=after-rendering-delay")
               .arg(token.frameId)
               .arg(QLatin1StringView(stageName(stage)))
               .arg(token.sampleId)
               .arg(pointerName(token.provider))
               .arg(token.clientBufferId)
               .arg(token.sourceBufferId)
               .arg(pointerName(token.texture));
    return token;
}

void cleanupRunning(const CleanupToken &token)
{
    if (!enabled() || (!token.sampleId && !token.sourceBufferId))
        return;

    FrameStage stage = FrameStage::Idle;
    {
        QMutexLocker locker(&s_mutex);
        if (s_providers.value(token.provider).sampleId == token.sampleId)
            s_providers.remove(token.provider);
        if (s_textures.value(token.texture).sampleId == token.sampleId)
            s_textures.remove(token.texture);
        auto windowIt = s_windows.find(token.window);
        if (windowIt != s_windows.end())
            stage = windowIt->stage;
    }
    if (!token.logLifecycle)
        return;
    qCDebug(lcWlQtQuickTexture).noquote()
        << QStringLiteral("VKTRACE event=cleanup-running frame=%1 stage=%2 sampleId=%3 provider=%4 clientBufferId=%5 sourceBufferId=%6 texture=%7 unlock=imminent")
               .arg(token.frameId)
               .arg(QLatin1StringView(stageName(stage)))
               .arg(token.sampleId)
               .arg(pointerName(token.provider))
               .arg(token.clientBufferId)
               .arg(token.sourceBufferId)
               .arg(pointerName(token.texture));
}

void qtWrap(bool begin, qw_texture *texture, quint64 image, quint32 vkFormat,
            int rhiFormat, const char *bridgeClass, const char *layout)
{
    if (!enabled() || !texture)
        return;

    const auto provider = providerRecordForTexture(reinterpret_cast<quintptr>(texture));
    if (!provider.sampleId || !provider.logQtWrap)
        return;

    quint64 frameId = 0;
    FrameStage stage = FrameStage::Idle;
    {
        QMutexLocker locker(&s_mutex);
        auto windowIt = s_windows.find(provider.window);
        if (windowIt != s_windows.end()) {
            frameId = windowIt->frameId;
            stage = windowIt->stage;
        }
    }

    qCDebug(lcWlQtQuickTexture).noquote()
        << QStringLiteral("VKTRACE event=%1 frame=%2 stage=%3 sampleId=%4 sourceBufferId=%5 texture=%6 image=%7 vkFormat=%8 rhiFormat=%9 bridgeClass=%10 layout=%11 result=%12")
               .arg(begin ? QStringLiteral("qt-wrap-begin") : QStringLiteral("qt-wrap-end"))
               .arg(frameId)
               .arg(QLatin1StringView(stageName(stage)))
               .arg(provider.sampleId)
               .arg(provider.sourceBufferId)
               .arg(pointerName(reinterpret_cast<quintptr>(texture)))
               .arg(hex64(image))
               .arg(hex32(vkFormat))
               .arg(rhiFormat)
               .arg(QLatin1StringView(bridgeClass ? bridgeClass : "unknown"))
               .arg(QLatin1StringView(layout ? layout : "unknown"))
               .arg(begin ? QStringLiteral("pending") : QStringLiteral("unobservable"));
}

void beginFrame(WOutputRenderWindow *window)
{
    if (!enabled() || !window)
        return;

    {
        QMutexLocker locker(&s_mutex);
        auto &record = s_windows[reinterpret_cast<quintptr>(window)];
        record = WindowRecord {};
        record.frameId = s_nextFrameId.fetch_add(1, std::memory_order_relaxed);
        record.stage = FrameStage::FrameBegin;
        record.frameTimer.start();
    }
}

void setFrameStage(WOutputRenderWindow *window, FrameStage stage)
{
    if (!enabled() || !window)
        return;

    {
        QMutexLocker locker(&s_mutex);
        auto &record = s_windows[reinterpret_cast<quintptr>(window)];
        record.stage = stage;
        if (stage == FrameStage::BeforeEndFrame)
            record.endFrameTimer.start();
        else if (stage == FrameStage::AfterEndFrame && record.endFrameTimer.isValid()) {
            record.endFrameUsec = record.endFrameTimer.nsecsElapsed() / 1000;
        }
    }
}

void endFrame(WOutputRenderWindow *window, qsizetype committedOutputCount)
{
    if (!enabled() || !window)
        return;

    QString message;
    bool unclosedPass = false;
    bool relevant = false;
    {
        QMutexLocker locker(&s_mutex);
        auto windowIt = s_windows.find(reinterpret_cast<quintptr>(window));
        if (windowIt == s_windows.end())
            return;
        auto &record = windowIt.value();
        unclosedPass = !record.passStack.isEmpty();
        relevant = !record.bufferIds.isEmpty()
                   || record.missingPrepareCount > 0
                   || record.releaseBeforeEndFrameCandidateCount > 0;
        auto bufferIds = record.bufferIds.values();
        std::sort(bufferIds.begin(), bufferIds.end());
        QStringList bufferIdNames;
        bufferIdNames.reserve(bufferIds.size());
        for (const auto id : std::as_const(bufferIds))
            bufferIdNames.append(QString::number(id));
        message = QStringLiteral("VKTRACE event=frame-summary frame=%1 window=%2 passCount=%3 prepareCount=%4 prepareFailures=%5 finishCount=%6 finishFailures=%7 footprintCount=%8 missingPrepare=%9 releaseBeforeEndFrameCandidates=%10 endFrameUsec=%11 committedOutputs=%12 bufferCount=%13 bufferIds=%14 frameUsec=%15")
                      .arg(record.frameId)
                      .arg(pointerName(reinterpret_cast<quintptr>(window)))
                      .arg(record.passCount)
                      .arg(record.prepareCount)
                      .arg(record.prepareFailureCount)
                      .arg(record.finishCount)
                      .arg(record.finishFailureCount)
                      .arg(record.footprintCount)
                      .arg(record.missingPrepareCount)
                      .arg(record.releaseBeforeEndFrameCandidateCount)
                      .arg(record.endFrameUsec)
                      .arg(committedOutputCount)
                      .arg(record.bufferIds.size())
                      .arg(bufferIdNames.join(QLatin1Char(',')))
                      .arg(record.frameTimer.isValid() ? record.frameTimer.nsecsElapsed() / 1000 : 0);
        record.stage = FrameStage::Idle;
        record.passStack.clear();
    }
    if (!relevant)
        return;
    if (unclosedPass)
        qCWarning(lcWlRenderHelper).noquote() << message << "unclosedPass=true";
    else
        qCDebug(lcWlRenderHelper).noquote() << message;
}

void windowDestroyed(WOutputRenderWindow *window)
{
    if (!enabled() || !window)
        return;

    const quintptr windowKey = reinterpret_cast<quintptr>(window);
    QMutexLocker locker(&s_mutex);
    s_windows.remove(windowKey);
    for (auto it = s_providers.begin(); it != s_providers.end();) {
        if (it->window == windowKey)
            it = s_providers.erase(it);
        else
            ++it;
    }
    for (auto it = s_textures.begin(); it != s_textures.end();) {
        if (it->window == windowKey)
            it = s_textures.erase(it);
        else
            ++it;
    }
}

void beginPass(WOutputRenderWindow *window, const void *renderer,
               qw_buffer *targetBuffer, const char *purpose,
               int sourceIndex, qsizetype providerCount)
{
    if (!enabled() || !window)
        return;

    PassRecord pass;
    {
        QMutexLocker locker(&s_mutex);
        auto &record = s_windows[reinterpret_cast<quintptr>(window)];
        pass.passId = s_nextPassId.fetch_add(1, std::memory_order_relaxed);
        pass.renderer = reinterpret_cast<quintptr>(renderer);
        pass.targetBuffer = reinterpret_cast<quintptr>(targetBuffer ? targetBuffer->handle() : nullptr);
        pass.purpose = purpose ? purpose : "unknown";
        pass.sourceIndex = sourceIndex;
        pass.providerCount = providerCount;
        record.passStack.append(pass);
        ++record.passCount;
    }
}

void sampleDisposition(WOutputRenderWindow *window, const void *provider,
                       qw_texture *texture, SampleDisposition disposition,
                       qint64 elapsedUsec)
{
    if (!enabled() || !window)
        return;

    quint64 frameId = 0;
    quint64 passId = 0;
    ProviderRecord providerRecord;
    {
        QMutexLocker locker(&s_mutex);
        auto &windowRecord = s_windows[reinterpret_cast<quintptr>(window)];
        if (windowRecord.passStack.isEmpty())
            return;
        auto &pass = windowRecord.passStack.last();
        const quintptr providerKey = reinterpret_cast<quintptr>(provider);
        const quintptr textureKey = reinterpret_cast<quintptr>(texture);
        providerRecord = s_providers.value(providerKey);
        if (!providerRecord.sampleId)
            return;
        pass.providerDispositions.insert(providerKey, disposition);
        if (disposition == SampleDisposition::Prepared) {
            if (textureKey)
                pass.preparedTextures.insert(textureKey);
        }
        if (disposition == SampleDisposition::Prepared) {
            ++pass.prepareCount;
            ++windowRecord.prepareCount;
        } else if (disposition == SampleDisposition::PrepareFailed) {
            ++pass.prepareFailureCount;
            ++windowRecord.prepareFailureCount;
        }
        windowRecord.bufferIds.insert(providerRecord.sourceBufferId);
        auto sourceIt = s_sourceBuffers.find(providerRecord.sourceBuffer);
        if (sourceIt != s_sourceBuffers.end()
            && sourceIt->sourceBufferId == providerRecord.sourceBufferId) {
            sourceIt->lastFrameId = windowRecord.frameId;
            sourceIt->lastWindow = reinterpret_cast<quintptr>(window);
        }
        frameId = windowRecord.frameId;
        passId = pass.passId;
    }

    if (disposition != SampleDisposition::PrepareFailed
        && !(disposition == SampleDisposition::Prepared && elapsedUsec > 1000)) {
        return;
    }

    const QString message = QStringLiteral("VKTRACE event=sample-prepare frame=%1 pass=%2 sampleId=%3 sourceBufferId=%4 provider=%5 texture=%6 disposition=%7 elapsedUsec=%8")
                                .arg(frameId)
                                .arg(passId)
                                .arg(providerRecord.sampleId)
                                .arg(providerRecord.sourceBufferId)
                                .arg(pointerName(reinterpret_cast<quintptr>(provider)))
                                .arg(pointerName(reinterpret_cast<quintptr>(texture)))
                                .arg(QLatin1StringView(dispositionName(disposition)))
                                .arg(elapsedUsec);
    if (disposition == SampleDisposition::PrepareFailed)
        qCWarning(lcWlQtQuickTexture).noquote() << message;
    else
        qCDebug(lcWlQtQuickTexture).noquote() << message;
}

void sampleFinished(WOutputRenderWindow *window, qw_texture *texture,
                    bool ok, qint64 elapsedUsec)
{
    if (!enabled() || !window || !texture)
        return;

    quint64 frameId = 0;
    quint64 passId = 0;
    ProviderRecord providerRecord;
    {
        QMutexLocker locker(&s_mutex);
        auto &windowRecord = s_windows[reinterpret_cast<quintptr>(window)];
        if (windowRecord.passStack.isEmpty())
            return;
        auto &pass = windowRecord.passStack.last();
        providerRecord = s_textures.value(reinterpret_cast<quintptr>(texture));
        if (!providerRecord.sampleId)
            return;
        ++pass.finishCount;
        ++windowRecord.finishCount;
        if (!ok) {
            ++pass.finishFailureCount;
            ++windowRecord.finishFailureCount;
        }
        frameId = windowRecord.frameId;
        passId = pass.passId;
    }

    if (ok && elapsedUsec <= 1000)
        return;

    const QString message = QStringLiteral("VKTRACE event=sample-finish frame=%1 pass=%2 sampleId=%3 sourceBufferId=%4 texture=%5 result=%6 elapsedUsec=%7")
                                .arg(frameId)
                                .arg(passId)
                                .arg(providerRecord.sampleId)
                                .arg(providerRecord.sourceBufferId)
                                .arg(pointerName(reinterpret_cast<quintptr>(texture)))
                                .arg(ok)
                                .arg(elapsedUsec);
    if (ok)
        qCDebug(lcWlQtQuickTexture).noquote() << message;
    else
        qCWarning(lcWlQtQuickTexture).noquote() << message;
}

void surfaceFootprint(WOutputRenderWindow *window, const void *provider,
                      qw_texture *texture, const void *surface)
{
    if (!enabled() || !window)
        return;

    QMutexLocker locker(&s_mutex);
    auto &windowRecord = s_windows[reinterpret_cast<quintptr>(window)];
    if (windowRecord.passStack.isEmpty())
        return;

    auto &pass = windowRecord.passStack.last();
    const quintptr providerKey = reinterpret_cast<quintptr>(provider);
    const quintptr textureKey = reinterpret_cast<quintptr>(texture);
    const auto providerRecord = s_providers.value(providerKey);
    if (!providerRecord.sampleId)
        return;

    ++pass.footprintCount;
    ++windowRecord.footprintCount;
    windowRecord.bufferIds.insert(providerRecord.sourceBufferId);
    auto sourceIt = s_sourceBuffers.find(providerRecord.sourceBuffer);
    if (sourceIt != s_sourceBuffers.end()
        && sourceIt->sourceBufferId == providerRecord.sourceBufferId) {
        sourceIt->lastDrawFrameId = windowRecord.frameId;
        sourceIt->lastFrameId = windowRecord.frameId;
        sourceIt->lastSampleId = providerRecord.sampleId;
        sourceIt->lastWindow = reinterpret_cast<quintptr>(window);
        sourceIt->lastDrawWindow = reinterpret_cast<quintptr>(window);
        sourceIt->lastSurface = reinterpret_cast<quintptr>(surface);
    }

    if (textureKey && pass.preparedTextures.contains(textureKey))
        return;

    if (!pass.providerDispositions.contains(providerKey)) {
        if (providerRecord.sampleId) {
            ++pass.missingProviderCount;
            ++windowRecord.missingPrepareCount;
        } else {
            ++pass.invalidProviderDrawCount;
        }
        return;
    }

    const auto disposition = pass.providerDispositions.value(providerKey);
    switch (disposition) {
    case SampleDisposition::PrepareFailed:
        ++pass.drawAfterPrepareFailureCount;
        ++windowRecord.missingPrepareCount;
        break;
    case SampleDisposition::CurrentRenderTarget:
        ++pass.currentTargetFeedbackCount;
        ++windowRecord.missingPrepareCount;
        break;
    case SampleDisposition::InvalidProvider:
        ++pass.invalidProviderDrawCount;
        break;
    case SampleDisposition::Prepared:
    case SampleDisposition::DuplicateTexture:
        ++pass.missingProviderCount;
        ++windowRecord.missingPrepareCount;
        break;
    }
}

void endPass(WOutputRenderWindow *window, bool ok)
{
    if (!enabled() || !window)
        return;

    QString message;
    bool invariantFailure = false;
    bool trackedActivity = false;
    {
        QMutexLocker locker(&s_mutex);
        auto &windowRecord = s_windows[reinterpret_cast<quintptr>(window)];
        if (windowRecord.passStack.isEmpty())
            return;
        const auto pass = windowRecord.passStack.takeLast();
        trackedActivity = !pass.providerDispositions.isEmpty() || pass.footprintCount > 0;
        invariantFailure = pass.missingProviderCount > 0
                           || pass.drawAfterPrepareFailureCount > 0
                           || pass.currentTargetFeedbackCount > 0
                           || pass.finishFailureCount > 0
                           || pass.prepareCount != pass.finishCount
                           || !ok;
        message = QStringLiteral("VKTRACE event=pass-end frame=%1 pass=%2 result=%3 prepareCount=%4 prepareFailures=%5 finishCount=%6 finishFailures=%7 footprintCount=%8 missingActiveProvider=%9 drawAfterPrepareFailure=%10 currentTargetFeedback=%11 invalidProviderDraw=%12")
                      .arg(windowRecord.frameId)
                      .arg(pass.passId)
                      .arg(ok)
                      .arg(pass.prepareCount)
                      .arg(pass.prepareFailureCount)
                      .arg(pass.finishCount)
                      .arg(pass.finishFailureCount)
                      .arg(pass.footprintCount)
                      .arg(pass.missingProviderCount)
                      .arg(pass.drawAfterPrepareFailureCount)
                      .arg(pass.currentTargetFeedbackCount)
                      .arg(pass.invalidProviderDrawCount);
    }
    if (!trackedActivity)
        return;
    if (invariantFailure)
        qCWarning(lcWlBufferRenderer).noquote() << message;
    else
        qCDebug(lcWlBufferRenderer).noquote() << message;
}

void outputCommitState(qw_output *output, quint32 committedState)
{
    if (!enabled() || !output || !output->handle())
        return;
    QMutexLocker locker(&s_mutex);
    auto &states = s_outputCommitStates[reinterpret_cast<quintptr>(output->handle())];
    states.insert(output->handle()->commit_seq, committedState);
    while (states.size() > MaxTrackedOutputCommits)
        states.erase(states.begin());
}

void outputCommitted(WOutputRenderWindow *window, qw_output *output,
                     quint32 sequenceBefore, bool ok)
{
    if (!enabled() || !window || !output || !output->handle())
        return;

    const quint32 sequenceAfter = output->handle()->commit_seq;
    const bool sequenceAdvanced = ok && sequenceAfter != sequenceBefore;
    quint64 frameId = 0;
    quint32 committedState = 0;
    bool relevant = false;
    {
        QMutexLocker locker(&s_mutex);
        const auto windowRecord = s_windows.value(reinterpret_cast<quintptr>(window));
        frameId = windowRecord.frameId;
        relevant = !windowRecord.bufferIds.isEmpty();
        if (!relevant)
            return;
        if (sequenceAdvanced) {
            committedState = s_outputCommitStates.value(reinterpret_cast<quintptr>(output->handle()))
                                 .value(sequenceAfter);
            auto &commits = s_outputCommits[reinterpret_cast<quintptr>(output->handle())];
            commits.insert(sequenceAfter, OutputCommitRecord { frameId, committedState });
            while (commits.size() > MaxTrackedOutputCommits)
                commits.erase(commits.begin());
        }
    }

    if (ok)
        return;

    const QString message = QStringLiteral("VKTRACE event=output-commit frame=%1 output=%2 seqBefore=%3 commitSeq=%4 seqAdvanced=%5 result=%6 stateKnown=%7 committedState=%8 bufferCommitted=%9")
                                .arg(frameId)
                                .arg(pointerName(reinterpret_cast<quintptr>(output->handle())))
                                .arg(sequenceBefore)
                                .arg(sequenceAfter)
                                .arg(sequenceAdvanced)
                                .arg(ok)
                                .arg(sequenceAdvanced)
                                .arg(hex32(committedState))
                                .arg(bool(committedState & WLR_OUTPUT_STATE_BUFFER));
    qCWarning(lcWlOutputBuffer).noquote() << message;
}

void outputPresented(qw_output *output, const wlr_output_event_present *event)
{
    if (!enabled() || !output || !output->handle() || !event)
        return;

    OutputCommitRecord commit;
    bool matched = false;
    {
        QMutexLocker locker(&s_mutex);
        auto outputIt = s_outputCommits.find(reinterpret_cast<quintptr>(output->handle()));
        if (outputIt != s_outputCommits.end()) {
            auto commitIt = outputIt->find(event->commit_seq);
            if (commitIt != outputIt->end()) {
                commit = commitIt.value();
                outputIt->erase(commitIt);
                matched = true;
            }
        }
    }

    qCDebug(lcWlOutputBuffer).noquote()
        << QStringLiteral("VKTRACE event=output-present frame=%1 output=%2 commitSeq=%3 matched=%4 presented=%5 when=%6.%7 refreshNs=%8 flags=%9 bufferCommitted=%10")
               .arg(commit.frameId)
               .arg(pointerName(reinterpret_cast<quintptr>(output->handle())))
               .arg(event->commit_seq)
               .arg(matched)
               .arg(event->presented)
               .arg(event->when.tv_sec)
               .arg(event->when.tv_nsec, 9, 10, QLatin1Char('0'))
               .arg(event->refresh)
               .arg(hex32(event->flags))
               .arg(bool(commit.committedState & WLR_OUTPUT_STATE_BUFFER));
}

void outputDestroyed(qw_output *output)
{
    if (!enabled() || !output || !output->handle())
        return;
    const quintptr outputKey = reinterpret_cast<quintptr>(output->handle());
    QMutexLocker locker(&s_mutex);
    s_outputCommits.remove(outputKey);
    s_outputCommitStates.remove(outputKey);
}

} // namespace WVulkanTrace

WAYLIB_SERVER_END_NAMESPACE

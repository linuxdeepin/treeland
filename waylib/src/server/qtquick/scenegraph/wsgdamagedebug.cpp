// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagedebug_p.h"
#include "wsgbatchrenderer_p.h"
#include "wsgdamagetracker_p.h"
#include "wayliblogging.h"

#include <private/qsgmaterialshader_p.h>

#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QPainter>
#include <QRect>
#include <QStringList>

#include <cstring>
#include <optional>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {

constexpr int kMaxDrawRects = 192;
constexpr int kBorderWidth = 2;
// Recycled swapchain images still hold the last overlay. Turning highlight
// off must full-redraw several frames, not only the leftover extra region
// once (grabToImage also switches to None and would otherwise clear entries
// before other images are erased).
constexpr int kSwapchainEraseFrames = 8;
constexpr int kUbufSize = 152; // visualization.vert/frag
constexpr float kFillRgb = 0.45f;
constexpr float kBorderRgb = 0.9f;

const QRhiShaderResourceBinding::StageFlags kUbufVisibility =
    QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;

QRect toDeviceRect(const QRect &sceneRect, const QTransform &sceneToDevice)
{
    return sceneToDevice.mapRect(QRectF(sceneRect)).toAlignedRect();
}

QRegion inflateRegion(const QRegion &region, int margin)
{
    if (region.isEmpty() || margin <= 0)
        return region;
    QRegion out;
    for (const QRect &rect : region)
        out += rect.adjusted(-margin, -margin, margin, margin);
    return out;
}

void fillPremul(float *out, float r, float g, float b, float alpha)
{
    const float a = qBound(0.0f, alpha, 1.0f);
    out[0] = r * a;
    out[1] = g * a;
    out[2] = b * a;
    out[3] = a;
}

void appendSceneQuad(QVector<float> &verts, const QRect &rect)
{
    if (rect.isEmpty())
        return;
    const float x0 = float(rect.x());
    const float x1 = float(rect.x() + rect.width());
    const float y0 = float(rect.y());
    const float y1 = float(rect.y() + rect.height());
    // vec4 so Vulkan does not leave w=0 (Float2 remainder is not reliable).
    verts << x0 << y0 << 0.0f << 1.0f
          << x1 << y0 << 0.0f << 1.0f
          << x0 << y1 << 0.0f << 1.0f
          << x1 << y1 << 0.0f << 1.0f;
}

std::optional<WSGDamageDebug::Mode> tryParseMode(QByteArray raw)
{
    raw = raw.trimmed().toLower();
    if (raw.isEmpty() || raw == "0" || raw == "none" || raw == "false" || raw == "off")
        return WSGDamageDebug::Mode::None;
    if (raw == "rerender")
        return WSGDamageDebug::Mode::Rerender;
    if (raw == "log" || raw == "print")
        return WSGDamageDebug::Mode::Log;
    if (raw == "highlight" || raw == "1" || raw == "true" || raw == "on")
        return WSGDamageDebug::Mode::Highlight;
    return std::nullopt;
}

WSGDamageDebug::Mode parseModeFromEnv(const QByteArray &raw)
{
    if (const auto parsed = tryParseMode(raw))
        return *parsed;
    // Unknown values keep the historical highlight behaviour.
    return WSGDamageDebug::Mode::Highlight;
}

const char *nameForMode(WSGDamageDebug::Mode mode)
{
    switch (mode) {
    case WSGDamageDebug::Mode::None:
        return "none";
    case WSGDamageDebug::Mode::Rerender:
        return "rerender";
    case WSGDamageDebug::Mode::Highlight:
        return "highlight";
    case WSGDamageDebug::Mode::Log:
        return "log";
    }
    return "none";
}

std::optional<WSGDamageDebug::Mode> g_modeOverride;

} // namespace

struct WSGDamageDebug::Overlay
{
    struct DrawCall {
        int vertexOffset = 0;
        float color[4] = {};
        int ubufOffset = 0;
    };

    QShader vs;
    QShader fs;
    QRhiBuffer *vbuf = nullptr;
    QRhiBuffer *ubuf = nullptr;
    QRhiGraphicsPipeline *ps = nullptr;
    QRhiShaderResourceBindings *srb = nullptr;
    QRhiRenderPassDescriptor *rpDesc = nullptr;
    QList<DrawCall> drawCalls;
    QRect scissor;
    int ubufAlign = 256;

    void reset()
    {
        delete ps;
        ps = nullptr;
        delete srb;
        srb = nullptr;
        delete ubuf;
        ubuf = nullptr;
        delete vbuf;
        vbuf = nullptr;
        rpDesc = nullptr;
        drawCalls.clear();
        scissor = QRect();
    }
};

WSGDamageDebug::Mode WSGDamageDebug::mode()
{
    static Mode parsed = Mode::None;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        parsed = parseModeFromEnv(qgetenv("WAYLIB_DEBUG_DAMAGE"));
        if (parsed != Mode::None)
            qCInfo(lcWlRenderer) << "WAYLIB_DEBUG_DAMAGE=" << nameForMode(parsed);
    }
    return g_modeOverride ? *g_modeOverride : parsed;
}

QString WSGDamageDebug::modeName()
{
    return QString::fromLatin1(nameForMode(mode()));
}

void WSGDamageDebug::setMode(Mode mode)
{
    const Mode previous = WSGDamageDebug::mode();
    g_modeOverride = mode;
    if (previous != mode)
        qCInfo(lcWlRenderer) << "WAYLIB_DEBUG_DAMAGE=" << nameForMode(mode);
}

bool WSGDamageDebug::setModeName(const QString &name)
{
    const auto parsed = tryParseMode(name.toUtf8());
    if (!parsed)
        return false;
    setMode(*parsed);
    return true;
}

QString WSGDamageDebug::describe(const QRegion &region, bool full)
{
    if (full)
        return QStringLiteral("full");
    QStringList parts;
    for (const QRect &rect : region) {
        parts << QStringLiteral("%1x%2+%3+%4")
                     .arg(rect.width())
                     .arg(rect.height())
                     .arg(rect.x())
                     .arg(rect.y());
    }
    return parts.isEmpty() ? QStringLiteral("empty") : parts.join(QLatin1Char(','));
}

qint64 WSGDamageDebug::nowMs()
{
    static const qint64 origin = [] {
        QElapsedTimer t;
        t.start();
        return t.msecsSinceReference();
    }();
    QElapsedTimer t;
    t.start();
    return t.msecsSinceReference() - origin;
}

WSGDamageDebug::WSGDamageDebug()
    : m_overlay(std::make_unique<Overlay>())
{
}

WSGDamageDebug::~WSGDamageDebug()
{
    releaseResources();
}

bool WSGDamageDebug::needsAnotherFrame() const
{
    if (mode() == Mode::Highlight)
        return !m_entries.isEmpty();
    if (mode() == Mode::None) {
        return m_eraseFullFrames > 0
            || !m_entries.isEmpty()
            || m_extraFull
            || !m_extra.isEmpty();
    }
    return false;
}

float WSGDamageDebug::alphaFor(const Entry &e, qint64 now)
{
    const qint64 age = qMax(qint64(0), now - e.whenMs);
    if (age >= fadeOutMs)
        return 0.0f;
    return 1.0f - float(age) / float(fadeOutMs);
}

void WSGDamageDebug::rgbForFrame(int index, int liveCount, float *r, float *g, float *b)
{
    const float t = (liveCount <= 1)
        ? 0.0f
        : float(qBound(0, index, liveCount - 1)) / float(liveCount - 1);
    *r = 1.0f - t;
    *g = t;
    *b = 0.0f;
}

int WSGDamageDebug::liveEntryCount(const QList<Entry> &entries, qint64 now)
{
    int n = 0;
    for (const Entry &e : entries) {
        if (alphaFor(e, now) > 0.0f)
            ++n;
    }
    return n;
}

QVector<QRect> WSGDamageDebug::borderRects(const QRect &rect, int width)
{
    QVector<QRect> out;
    if (rect.isEmpty() || width <= 0)
        return out;
    const QRect outer = rect.adjusted(-width, -width, width, width);
    out.append(QRect(outer.x(), rect.y() + rect.height(), outer.width(), width));
    out.append(QRect(outer.x(), rect.y() - width, outer.width(), width));
    out.append(QRect(rect.x() - width, rect.y(), width, rect.height()));
    out.append(QRect(rect.x() + rect.width(), rect.y(), width, rect.height()));
    return out;
}

void WSGDamageDebug::rebuildEntries(qint64 now)
{
    QRegion acc;
    bool accFull = false;
    QList<Entry> kept;
    kept.reserve(m_entries.size());

    for (Entry &e : m_entries) {
        if (!accFull) {
            if (e.full) {
                accFull = true;
                acc = QRegion();
            } else {
                e.region -= acc;
                acc += e.region;
            }
        } else {
            e.region = QRegion();
            e.full = false;
        }

        const bool expired = (now - e.whenMs) >= fadeOutMs
            || (!e.full && e.region.isEmpty());
        if (!expired)
            kept.append(e);
    }

    m_entries = kept;
    m_extra = accFull ? QRegion() : inflateRegion(acc, kBorderWidth);
    m_extraFull = accFull;
    m_nowMs = now;
}

void WSGDamageDebug::addFrame(const QRegion &content, bool full, qint64 now)
{
    if (full || !content.isEmpty()) {
        Entry e;
        e.region = full ? QRegion() : content;
        e.whenMs = now;
        e.full = full;
        m_entries.prepend(e);
    }
    rebuildEntries(now);
}

void WSGDamageDebug::applyToTracker(WSGDamageTracker *tracker, qint64 now)
{
    Q_ASSERT(tracker);
    switch (mode()) {
    case Mode::None: {
        // Recycled Preserve buffers still show the last overlay. A single
        // extra-region dirty only redraws the current image; grabToImage also
        // switches to None and would clear leftover before other images are
        // erased. Full-redraw a few frames so the whole swapchain is clean.
        const bool leftover = !m_entries.isEmpty() || m_extraFull || !m_extra.isEmpty();
        if (leftover || m_eraseFullFrames > 0) {
            tracker->markFull();
            if (leftover) {
                m_entries.clear();
                m_extra = QRegion();
                m_extraFull = false;
                m_eraseFullFrames = kSwapchainEraseFrames - 1;
            } else {
                --m_eraseFullFrames;
            }
        }
        return;
    }
    case Mode::Rerender:
        tracker->markFull();
        return;
    case Mode::Log:
        return;
    case Mode::Highlight:
        break;
    }

    const bool full = tracker->pendingIsFull();
    const QRegion content = full ? QRegion() : tracker->pendingRegion();
    addFrame(content, full, now);
    if (m_extraFull)
        tracker->markFull();
    else
        tracker->addRegion(m_extra);
}

void WSGDamageDebug::paint(QPainter *painter, const QTransform &sceneToDevice) const
{
    paint(painter, sceneToDevice, m_entries, m_nowMs);
}

void WSGDamageDebug::paint(QPainter *painter, const QTransform &sceneToDevice,
                           const QList<Entry> &entries, qint64 now)
{
    if (!painter || entries.isEmpty())
        return;

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setRenderHint(QPainter::Antialiasing, false);

    const QRect clip = painter->device()
        ? QRect(0, 0, painter->device()->width(), painter->device()->height())
        : QRect();

    const int liveCount = liveEntryCount(entries, now);
    int index = 0;
    for (const Entry &e : entries) {
        const float alpha = alphaFor(e, now);
        if (alpha <= 0.0f)
            continue;

        float red = 1.0f;
        float green = 0.0f;
        float blue = 0.0f;
        rgbForFrame(index, liveCount, &red, &green, &blue);
        ++index;

        QColor fill(qRound(255.0f * red), qRound(255.0f * green), qRound(255.0f * blue),
                    qRound(255.0f * kFillRgb * alpha));
        QColor border(qRound(255.0f * red), qRound(255.0f * green), qRound(255.0f * blue),
                      qRound(255.0f * kBorderRgb * alpha));

        QVector<QRect> rects;
        if (e.full) {
            rects.append(clip.isNull() ? QRect(0, 0, 1, 1) : clip);
        } else {
            for (const QRect &r : e.region)
                rects.append(toDeviceRect(r, sceneToDevice));
        }

        painter->setBrush(fill);
        for (const QRect &r : rects) {
            const QRect clipped = clip.isNull() ? r : r.intersected(clip);
            if (!clipped.isEmpty())
                painter->fillRect(clipped, fill);
        }
        painter->setBrush(border);
        for (const QRect &r : rects) {
            for (const QRect &b : borderRects(r, kBorderWidth)) {
                const QRect clipped = clip.isNull() ? b : b.intersected(clip);
                if (!clipped.isEmpty())
                    painter->fillRect(clipped, border);
            }
        }
    }
    painter->restore();
}

void WSGDamageDebug::prepareOverlay(WSGBatchRenderer::Renderer *renderer,
                                    QRhi *rhi,
                                    QRhiResourceUpdateBatch *u)
{
    Overlay &ov = *m_overlay;
    ov.drawCalls.clear();
    ov.scissor = QRect();
    if (!renderer || !rhi || !u || mode() != Mode::Highlight || m_entries.isEmpty())
        return;

    if (!ov.vs.isValid()) {
        ov.vs = QSGMaterialShaderPrivate::loadShader(
            QLatin1String(":/qt-project.org/scenegraph/shaders_ng/visualization.vert.qsb"));
        ov.fs = QSGMaterialShaderPrivate::loadShader(
            QLatin1String(":/qt-project.org/scenegraph/shaders_ng/visualization.frag.qsb"));
    }
    if (!ov.vs.isValid() || !ov.fs.isValid())
        return;

    const QRect device = renderer->deviceRect();
    if (device.width() <= 0 || device.height() <= 0)
        return;
    // Same clip space as the scene batches (Y-up). Native NDC is Y-down on
    // Vulkan and would miss the QRhi viewport correction.
    const QMatrix4x4 projection = renderer->projectionMatrix(0);

    struct Pending {
        QRect rect;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
    };
    QVector<Pending> pending;
    pending.reserve(64);

    auto addRect = [&](QRect r, float red, float green, float blue, float alpha) {
        r = r.intersected(device);
        if (r.isEmpty() || pending.size() >= kMaxDrawRects)
            return;
        pending.append(Pending{ r, red, green, blue, alpha });
    };

    const int liveCount = liveEntryCount(m_entries, m_nowMs);
    int index = 0;
    for (const Entry &e : m_entries) {
        const float alpha = alphaFor(e, m_nowMs);
        if (alpha <= 0.0f)
            continue;

        float red = 1.0f;
        float green = 0.0f;
        float blue = 0.0f;
        rgbForFrame(index, liveCount, &red, &green, &blue);
        ++index;

        QVector<QRect> rects;
        if (e.full) {
            rects.append(device);
        } else {
            for (const QRect &sceneRect : e.region) {
                if (!sceneRect.isEmpty())
                    rects.append(sceneRect);
            }
        }
        for (const QRect &r : rects) {
            addRect(r, red, green, blue, kFillRgb * alpha);
            for (const QRect &border : borderRects(r, kBorderWidth))
                addRect(border, red, green, blue, kBorderRgb * alpha);
        }
    }

    if (pending.isEmpty())
        return;

    QRect cover;
    bool mapped = true;
    for (const Pending &p : pending) {
        const QRect native = renderer->sceneRectToNativeScissor(p.rect);
        if (native.isEmpty()) {
            mapped = false;
            break;
        }
        cover = cover.united(native);
    }
    ov.scissor = (mapped && !cover.isEmpty()) ? cover : device;

    QVector<float> verts;
    verts.reserve(pending.size() * 16);
    for (const Pending &p : pending)
        appendSceneQuad(verts, p.rect);

    const quint32 vbytes = quint32(verts.size() * int(sizeof(float)));
    if (!ov.vbuf || ov.vbuf->size() < vbytes) {
        delete ov.vbuf;
        ov.vbuf = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                 qMax(vbytes, quint32(128)));
        if (!ov.vbuf->create()) {
            delete ov.vbuf;
            ov.vbuf = nullptr;
            return;
        }
    }
    u->updateDynamicBuffer(ov.vbuf, 0, vbytes, verts.constData());

    ov.ubufAlign = qMax(rhi->ubufAlignment(), 256);
    const quint32 ubufBytes = quint32(pending.size() * ov.ubufAlign);
    if (!ov.ubuf || ov.ubuf->size() < ubufBytes) {
        delete ov.ubuf;
        ov.ubuf = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, ubufBytes);
        if (!ov.ubuf->create()) {
            delete ov.ubuf;
            ov.ubuf = nullptr;
            return;
        }
        delete ov.srb;
        ov.srb = nullptr;
        delete ov.ps;
        ov.ps = nullptr;
    }

    QByteArray ubuf(int(ubufBytes), 0);
    QMatrix4x4 ident;
    const float pattern = 0.0f;
    const qint32 projectionFlag = 0;
    ov.drawCalls.reserve(pending.size());
    for (int i = 0; i < pending.size(); ++i) {
        Overlay::DrawCall dc;
        dc.vertexOffset = i * 4 * int(sizeof(float)) * 4;
        dc.ubufOffset = i * ov.ubufAlign;
        fillPremul(dc.color, pending.at(i).r, pending.at(i).g, pending.at(i).b, pending.at(i).a);
        char *slot = ubuf.data() + dc.ubufOffset;
        memcpy(slot + 0, projection.constData(), 64);
        memcpy(slot + 64, ident.constData(), 64);
        memcpy(slot + 128, dc.color, 16);
        memcpy(slot + 144, &pattern, 4);
        memcpy(slot + 148, &projectionFlag, 4);
        ov.drawCalls.append(dc);
    }
    u->updateDynamicBuffer(ov.ubuf, 0, ubufBytes, ubuf.constData());

    if (!ov.srb) {
        ov.srb = rhi->newShaderResourceBindings();
        ov.srb->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0, kUbufVisibility, ov.ubuf, kUbufSize)
        });
        if (!ov.srb->create()) {
            delete ov.srb;
            ov.srb = nullptr;
            ov.drawCalls.clear();
            return;
        }
    }

    QRhiRenderPassDescriptor *rpDesc = renderer->renderTarget().rpDesc;
    if (ov.ps && !ov.ps->flags().testFlag(QRhiGraphicsPipeline::UsesScissor)) {
        delete ov.ps;
        ov.ps = nullptr;
    }
    if (!ov.ps || ov.rpDesc != rpDesc) {
        delete ov.ps;
        ov.ps = rhi->newGraphicsPipeline();
        ov.ps->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        ov.ps->setTargetBlends({ blend });
        ov.ps->setDepthTest(false);
        ov.ps->setDepthWrite(false);
        ov.ps->setCullMode(QRhiGraphicsPipeline::None);
        ov.ps->setShaderStages({
            { QRhiShaderStage::Vertex, ov.vs },
            { QRhiShaderStage::Fragment, ov.fs }
        });
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({ { 4 * sizeof(float) } });
        inputLayout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float4, 0 } });
        ov.ps->setVertexInputLayout(inputLayout);
        ov.ps->setShaderResourceBindings(ov.srb);
        ov.ps->setRenderPassDescriptor(rpDesc);
        ov.ps->setSampleCount(renderer->renderTarget().rt->sampleCount());
        // Vulkan keeps the last scene-batch dynamic scissor. Cover the
        // overlay quads in native space so leftover damage scissors cannot
        // clip the highlight. Mapping failure falls back to the device rect.
        ov.ps->setFlags(QRhiGraphicsPipeline::UsesScissor);
        if (!ov.ps->create()) {
            qCWarning(lcWlRenderer) << "Failed to create damage highlight pipeline";
            delete ov.ps;
            ov.ps = nullptr;
            ov.drawCalls.clear();
            return;
        }
        ov.rpDesc = rpDesc;
    }
}

void WSGDamageDebug::renderOverlay(WSGBatchRenderer::Renderer *renderer, QRhiCommandBuffer *cb)
{
    Overlay &ov = *m_overlay;
    if (!cb || !ov.ps || !ov.srb || !ov.vbuf || ov.drawCalls.isEmpty() || !renderer)
        return;

    cb->debugMarkBegin(QByteArrayLiteral("WAYLIB damage highlight"));
    cb->setGraphicsPipeline(ov.ps);
    cb->setViewport(renderer->m_pstate.viewport);
    QRect scissor = ov.scissor;
    if (scissor.isEmpty())
        scissor = renderer->deviceRect();
    cb->setScissor(QRhiScissor(scissor.x(), scissor.y(), scissor.width(), scissor.height()));

    for (const Overlay::DrawCall &dc : ov.drawCalls) {
        const QRhiCommandBuffer::DynamicOffset dyn(0, quint32(dc.ubufOffset));
        cb->setShaderResources(ov.srb, 1, &dyn);
        const QRhiCommandBuffer::VertexInput vb(ov.vbuf, quint32(dc.vertexOffset));
        cb->setVertexInput(0, 1, &vb);
        cb->draw(4);
    }
    cb->debugMarkEnd();
}

void WSGDamageDebug::releaseResources()
{
    if (m_overlay)
        m_overlay->reset();
}

WAYLIB_SERVER_END_NAMESPACE

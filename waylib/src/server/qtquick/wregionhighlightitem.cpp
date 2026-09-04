// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wregionhighlightitem.h"
#include "woutputlayer.h"
#include "woutputviewport.h"
#include "woutputrenderwindow.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTransform>
#include <QVector>
#include <algorithm>

#include <private/qquickrectangle_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {

constexpr int kBorderWidth = 2;
constexpr float kFillRgb = 0.45f;
constexpr float kBorderRgb = 0.9f;

QRect toDeviceRect(const QRect &sceneRect, const QTransform &sceneToDevice)
{
    return sceneToDevice.mapRect(QRectF(sceneRect)).toAlignedRect();
}

} // namespace

qint64 WRegionOverlay::nowMs()
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

float WRegionOverlay::alphaFor(const Entry &e, qint64 now)
{
    const qint64 age = qMax(qint64(0), now - e.whenMs);
    if (age >= fadeOutMs)
        return 0.0f;
    return 1.0f - float(age) / float(fadeOutMs);
}

void WRegionOverlay::rgbForFrame(int index, int liveCount, float *r, float *g, float *b)
{
    const float t = (liveCount <= 1)
        ? 0.0f
        : float(qBound(0, index, liveCount - 1)) / float(liveCount - 1);
    *r = 1.0f - t;
    *g = t;
    *b = 0.0f;
}

int WRegionOverlay::liveEntryCount(const QList<Entry> &entries, qint64 now)
{
    int n = 0;
    for (const Entry &e : entries) {
        if (alphaFor(e, now) > 0.0f)
            ++n;
    }
    return n;
}

QVector<QRect> WRegionOverlay::borderRects(const QRect &rect, int width)
{
    QVector<QRect> out;
    if (rect.isEmpty() || width <= 0)
        return out;
    out.append(QRect(rect.x(), rect.y(), rect.width(), width));
    out.append(QRect(rect.x(), rect.y() + rect.height() - width, rect.width(), width));
    out.append(QRect(rect.x(), rect.y(), width, rect.height()));
    out.append(QRect(rect.x() + rect.width() - width, rect.y(), width, rect.height()));
    return out;
}

void WRegionOverlay::rebuildEntries(qint64 now)
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
    m_nowMs = now;
}

void WRegionOverlay::addFrame(const QRegion &content, bool full, qint64 now)
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

void WRegionOverlay::expire(qint64 now)
{
    rebuildEntries(now);
}

QVector<WRegionOverlay::OverlayRect> WRegionOverlay::overlayRects(const QRect &outputRect, qint64 now) const
{
    QVector<OverlayRect> out;
    const int liveCount = liveEntryCount(m_entries, now);
    int index = 0;
    for (const Entry &e : m_entries) {
        const float alpha = alphaFor(e, now);
        if (alpha <= 0.0f)
            continue;
        float red = 1.0f;
        float green = 0.0f;
        float blue = 0.0f;
        rgbForFrame(index, liveCount, &red, &green, &blue);
        ++index;
        OverlayRect quad;
        quad.fill = QColor(qRound(255.0f * red), qRound(255.0f * green), qRound(255.0f * blue),
                           qRound(255.0f * kFillRgb * alpha));
        quad.border = QColor(qRound(255.0f * red), qRound(255.0f * green), qRound(255.0f * blue),
                             qRound(255.0f * kBorderRgb * alpha));
        if (e.full) {
            if (outputRect.isEmpty())
                continue;
            quad.rect = outputRect;
            out.append(quad);
            continue;
        }
        for (const QRect &rect : e.region) {
            if (rect.isEmpty())
                continue;
            quad.rect = rect;
            out.append(quad);
        }
    }
    std::reverse(out.begin(), out.end());
    return out;
}

void WRegionOverlay::paint(QPainter *painter, const QTransform &sceneToDevice,
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
    QVector<const Entry *> live;
    live.reserve(liveCount);
    for (const Entry &e : entries) {
        if (alphaFor(e, now) > 0.0f)
            live.append(&e);
    }

    for (int i = live.size() - 1; i >= 0; --i) {
        const Entry &e = *live.at(i);
        const float alpha = alphaFor(e, now);
        float red = 1.0f;
        float green = 0.0f;
        float blue = 0.0f;
        rgbForFrame(i, live.size(), &red, &green, &blue);

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

WRegionHighlightItem::WRegionHighlightItem(WOutputViewport *viewport, QQuickItem *host)
    : QQuickItem(host)
    , m_viewport(viewport)
{
    Q_ASSERT(viewport);
    Q_ASSERT(host);
    setObjectName(QStringLiteral("damageHighlight"));
    setZ(1000000);
    setVisible(false);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptTouchEvents(false);
    setAcceptHoverEvents(false);
    if (auto *ctx = qmlContext(host))
        QQmlEngine::setContextForObject(this, ctx);

    m_layer = WOutputLayer::qmlAttachedProperties(this);
    // keepLayer: do not drop the overlay if the DRM plane is rejected.
    // force: software fallback must use the shadow copy, never extraRenderSource
    // onto the preserved primary buffer (that would dirty its damage ring).
    m_layer->setKeepLayer(true);
    m_layer->setForce(true);
    m_layer->setZ(1000000);
    m_layer->setOutputs({ viewport });
}

void WRegionHighlightItem::addFrame(const QRegion &content, bool full)
{
    m_overlay.addFrame(content, full);
}

QQuickRectangle *WRegionHighlightItem::ensureRect(int index)
{
    while (m_rects.size() <= index) {
        auto *rect = new QQuickRectangle(this);
        if (auto *ctx = qmlContext(this))
            QQmlEngine::setContextForObject(rect, ctx);
        rect->setVisible(false);
        rect->border()->setWidth(WRegionOverlay::overlayBorderWidth);
        m_rects.append(rect);
    }
    return m_rects.at(index);
}

void WRegionHighlightItem::setLayerEnabled(bool on)
{
    if (!m_layer || m_wantLayer == on)
        return;
    m_wantLayer = on;
    if (m_layer->enabled() != on)
        m_layer->setEnabled(on);
}

void WRegionHighlightItem::hide()
{
    for (auto *rect : std::as_const(m_rects))
        rect->setVisible(false);
    if (isVisible())
        setVisible(false);
    setLayerEnabled(false);
}

void WRegionHighlightItem::sync()
{
    auto *win = qobject_cast<WOutputRenderWindow *>(window());
    if (!m_viewport || !win
        || win->damageVisual() != WOutputRenderWindow::DamageVisual::Highlight) {
        hide();
        return;
    }

    m_overlay.expire();

    const qreal dpr = qMax(qreal(1), m_viewport->devicePixelRatio());
    const QRect output(0, 0,
                       qMax(1, qRound(m_viewport->width() * dpr)),
                       qMax(1, qRound(m_viewport->height() * dpr)));
    const auto quads = m_overlay.overlayRects(output);
    if (quads.isEmpty()) {
        hide();
        return;
    }

    QRect aabb;
    for (const auto &quad : quads)
        aabb |= quad.rect;
    if (aabb.isEmpty()) {
        hide();
        return;
    }

    const QPointF origin = m_viewport->position();
    setPosition(origin + QPointF(aabb.x(), aabb.y()) / dpr);
    setSize(QSizeF(aabb.width(), aabb.height()) / dpr);

    for (int i = 0; i < quads.size(); ++i) {
        auto *rect = ensureRect(i);
        const QRect &device = quads.at(i).rect;
        rect->setX((device.x() - aabb.x()) / dpr);
        rect->setY((device.y() - aabb.y()) / dpr);
        rect->setWidth(device.width() / dpr);
        rect->setHeight(device.height() / dpr);
        rect->setColor(quads.at(i).fill);
        rect->border()->setColor(quads.at(i).border);
        rect->setVisible(true);
    }
    for (int i = quads.size(); i < m_rects.size(); ++i)
        m_rects.at(i)->setVisible(false);

    // Extract from the parent scene before becoming visible so the primary
    // pass cannot paint these rectangles into the main buffer.
    setLayerEnabled(true);
    if (!isVisible())
        setVisible(true);
}

WAYLIB_SERVER_END_NAMESPACE

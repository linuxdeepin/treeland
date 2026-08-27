// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "private/wregionhighlightitem_p.h"
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

QRect toDeviceRect(const QRect &sceneRect, const QTransform &sceneToDevice)
{
    return sceneToDevice.mapRect(QRectF(sceneRect)).toAlignedRect();
}

// wlr_scene highlight: premultiplied { r = a*0.5, a = a*0.5 }.
// Qt blends an unpremultiplied color, so that is red at opacity a*0.5.
QColor highlightColor(float fade)
{
    const int alpha = qRound(255.0f * qBound(0.0f, fade, 1.0f) * 0.5f);
    return QColor(255, 0, 0, alpha);
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

void WRegionOverlay::rebuildEntries(qint64 now)
{
    WPixmanRegion acc;
    bool accFull = false;
    QList<Entry> kept;
    kept.reserve(m_entries.size());

    for (Entry &e : m_entries) {
        if (!accFull) {
            if (e.full) {
                accFull = true;
                acc.clear();
            } else {
                e.region -= acc;
                acc += e.region;
            }
        } else {
            e.region.clear();
            e.full = false;
        }

        const bool expired = (now - e.whenMs) >= fadeOutMs
            || (!e.full && e.region.isEmpty());
        if (!expired)
            kept.append(std::move(e));
    }

    m_entries = std::move(kept);
    m_nowMs = now;
}

void WRegionOverlay::addFrame(const WDamageRegion &damage, qint64 now)
{
    if (damage.isFull || !damage.region.isEmpty()) {
        Entry e;
        e.region = damage.isFull ? WPixmanRegion() : damage.region;
        e.whenMs = now;
        e.full = damage.isFull;
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
    for (const Entry &e : m_entries) {
        const float alpha = alphaFor(e, now);
        if (alpha <= 0.0f)
            continue;
        OverlayRect quad;
        quad.fill = highlightColor(alpha);
        quad.border = quad.fill;
        if (e.full) {
            if (outputRect.isEmpty())
                continue;
            quad.rect = outputRect;
            out.append(quad);
            continue;
        }
        int count = 0;
        const pixman_box32_t *boxes = e.region.rectangles(&count);
        for (int i = 0; i < count && boxes; ++i) {
            const QRect rect(boxes[i].x1, boxes[i].y1,
                             boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1);
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

    for (const Entry &e : entries) {
        const float alpha = alphaFor(e, now);
        if (alpha <= 0.0f)
            continue;
        const QColor fill = highlightColor(alpha);

        QVector<QRect> rects;
        if (e.full) {
            rects.append(clip.isNull() ? QRect(0, 0, 1, 1) : clip);
        } else {
            int count = 0;
            const pixman_box32_t *boxes = e.region.rectangles(&count);
            for (int i = 0; i < count && boxes; ++i) {
                const QRect r(boxes[i].x1, boxes[i].y1,
                              boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1);
                rects.append(toDeviceRect(r, sceneToDevice));
            }
        }
        for (const QRect &r : rects) {
            const QRect clipped = clip.isNull() ? r : r.intersected(clip);
            if (!clipped.isEmpty())
                painter->fillRect(clipped, fill);
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
    // Clear every repaint: while damage keeps arriving, needsAnotherFrame()
    // drives a repaint per frame for the whole 250ms fade. Recycled swapchain
    // images keep last-use pixels, so blending the current quads (alpha <= 0.5)
    // on top with SourceOver would otherwise accumulate coat over coat and the
    // overlay would saturate to opaque red within a few frames.
    m_layer->setColorContentsMode(WGlobal::ColorContentsMode::Clear);
    m_layer->setForce(true);
    m_layer->setZ(1000000);
    m_layer->setOutputs({ viewport });
}

void WRegionHighlightItem::addFrame(const WDamageRegion &damage)
{
    m_overlay.addFrame(damage);
}

QQuickRectangle *WRegionHighlightItem::ensureRect(int index)
{
    while (m_rects.size() <= index) {
        auto *rect = new QQuickRectangle(this);
        if (auto *ctx = qmlContext(this))
            QQmlEngine::setContextForObject(rect, ctx);
        rect->setVisible(false);
        rect->border()->setWidth(0);
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
    auto *win = qobject_cast<WOutputRenderWindow*>(window());
    if (!m_viewport || !win
        || win->damageVisual() != WOutputRenderWindow::DamageVisual::Highlight) {
        m_overlay.expire();
        m_overlay.clear();
        hide();
        return;
    }

    // Entries and lastRenderRegion() are scene-logical; keep the quads in
    // the same space. Device conversion happens only at damage-ring write.
    const QRect output(0, 0,
                       qMax(1, qRound(m_viewport->width())),
                       qMax(1, qRound(m_viewport->height())));
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
    setPosition(origin + QPointF(aabb.x(), aabb.y()));
    setSize(QSizeF(aabb.width(), aabb.height()));

    for (int i = 0; i < quads.size(); ++i) {
        auto *rect = ensureRect(i);
        const QRect &scene = quads.at(i).rect;
        rect->setX(scene.x() - aabb.x());
        rect->setY(scene.y() - aabb.y());
        rect->setWidth(scene.width());
        rect->setHeight(scene.height());
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

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QColor>
#include <QList>
#include <QQuickItem>
#include <QRegion>
#include <QVector>

QT_BEGIN_NAMESPACE
class QPainter;
class QQuickRectangle;
class QTransform;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewport;
class WOutputLayer;

// Fade history for the highlight layer. Not part of damage tracking.
class WRegionOverlay
{
public:
    static constexpr int fadeOutMs = 250;
    static constexpr int overlayBorderWidth = 2;

    struct Entry {
        QRegion region;
        qint64 whenMs = 0;
        bool full = false;
    };

    struct OverlayRect {
        QRect rect;
        QColor fill;
        QColor border;
    };

    static qint64 nowMs();

    void addFrame(const QRegion &content, bool full, qint64 now = nowMs());
    void expire(qint64 now = nowMs());
    bool needsAnotherFrame() const { return !m_entries.isEmpty(); }

    const QList<Entry> &entries() const { return m_entries; }
    qint64 currentTimeMs() const { return m_nowMs; }
    QVector<OverlayRect> overlayRects(const QRect &outputRect, qint64 now = nowMs()) const;

    static void paint(QPainter *painter, const QTransform &sceneToDevice,
                      const QList<Entry> &entries, qint64 now);

private:
    void rebuildEntries(qint64 now);
    static float alphaFor(const Entry &e, qint64 now);
    static int liveEntryCount(const QList<Entry> &entries, qint64 now);
    static void rgbForFrame(int index, int liveCount, float *r, float *g, float *b);
    static QVector<QRect> borderRects(const QRect &rect, int width);

    QList<Entry> m_entries;
    qint64 m_nowMs = 0;
};

// OutputLayer host. Damage quads are pooled QQuickRectangle children,
// sized to the overlay AABB so the layer buffer is not fullscreen.
// force+keepLayer: DRM overlay when possible; otherwise a shadow copy.
// Never extraRenderSource onto the preserved primary buffer.
class WRegionHighlightItem : public QQuickItem
{
    Q_OBJECT

public:
    explicit WRegionHighlightItem(WOutputViewport *viewport, QQuickItem *host);
    void addFrame(const QRegion &content, bool full);
    void sync();
    WRegionOverlay *overlay() { return &m_overlay; }
    const WRegionOverlay *overlay() const { return &m_overlay; }

private:
    QQuickRectangle *ensureRect(int index);
    void hide();
    void setLayerEnabled(bool on);

    WOutputViewport *m_viewport = nullptr;
    WOutputLayer *m_layer = nullptr;
    WRegionOverlay m_overlay;
    QVector<QQuickRectangle *> m_rects;
    bool m_wantLayer = false;
};

WAYLIB_SERVER_END_NAMESPACE

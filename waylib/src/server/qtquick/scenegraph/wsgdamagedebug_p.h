// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QList>
#include <QRegion>
#include <QString>
#include <QTransform>
#include <QVector>

#include <memory>

QT_BEGIN_NAMESPACE
class QPainter;
class QRhi;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGDamageTracker;

namespace WSGBatchRenderer {
class Renderer;
}

// Overlay that mirrors wlroots WLR_SCENE_DEBUG_DAMAGE (none / rerender / highlight).
// Controlled by WAYLIB_DEBUG_DAMAGE.
class WAYLIB_SERVER_EXPORT WSGDamageDebug
{
public:
    enum class Mode {
        None,
        Rerender,
        Highlight,
        Log,
    };

    static constexpr int fadeOutMs = 250;

    struct Entry {
        QRegion region;
        qint64 whenMs = 0;
        bool full = false;
    };

    static Mode mode();
    static QString modeName();
    static void setMode(Mode mode);
    static bool setModeName(const QString &name);
    static QString describe(const QRegion &region, bool full);
    static qint64 nowMs();

    WSGDamageDebug();
    ~WSGDamageDebug();

    bool needsAnotherFrame() const;

    // Snapshot tracker pending damage, push a highlight entry, expire old
    // overlays, and union remaining (including just-expired) rects back into
    // the tracker so those pixels are redrawn as the overlay fades.
    void applyToTracker(WSGDamageTracker *tracker, qint64 now = nowMs());

    // Software path: record this frame's content damage and compute extra
    // dirty for the next paint. extraDamage() is in the same coordinate
    // space as `content`.
    void addFrame(const QRegion &content, bool full, qint64 now = nowMs());
    QRegion extraDamage() const { return m_extra; }
    bool extraIsFull() const { return m_extraFull; }
    const QList<Entry> &entries() const { return m_entries; }
    qint64 currentTimeMs() const { return m_nowMs; }

    void paint(QPainter *painter, const QTransform &sceneToDevice) const;
    static void paint(QPainter *painter, const QTransform &sceneToDevice,
                      const QList<Entry> &entries, qint64 now);

    void prepareOverlay(WSGBatchRenderer::Renderer *renderer,
                        QRhi *rhi,
                        QRhiResourceUpdateBatch *u);
    void renderOverlay(WSGBatchRenderer::Renderer *renderer, QRhiCommandBuffer *cb);
    void releaseResources();

private:
    void rebuildEntries(qint64 now);
    static float alphaFor(const Entry &e, qint64 now);
    static int liveEntryCount(const QList<Entry> &entries, qint64 now);
    static void rgbForFrame(int index, int liveCount, float *r, float *g, float *b);
    static QVector<QRect> borderRects(const QRect &rect, int width);

    QList<Entry> m_entries;
    QRegion m_extra;
    bool m_extraFull = false;
    int m_eraseFullFrames = 0;
    qint64 m_nowMs = 0;

    struct Overlay;
    std::unique_ptr<Overlay> m_overlay;
};

WAYLIB_SERVER_END_NAMESPACE

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "damageclient.h"
#include "helper.h"
#include "wbufferrenderer_p.h"
#include "woutputviewport_p.h"
#include "wsgbatchrenderer_p.h"
#include "wsgdamagedebug_p.h"

#include <wbufferdumper.h>
#include <wcursor.h>
#include <wlogging.h>
#include <woutput.h>
#include <woutputitem.h>
#include <woutputlayer.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wrenderhelper.h>
#include <wserver.h>
#include <wsurface.h>
#include <wsurfaceitem.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFuture>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QtConcurrent>
#include <QtQml>

#include <cstdlib>

WAYLIB_SERVER_USE_NAMESPACE

namespace {

constexpr int kRgbTolerance = 40;
constexpr int kMaxMismatchPixels = 96;
constexpr int kOverlayPad = 4;
constexpr int kCursorPad = 8;
const QPointF kCursorHome(240, 300);
const QPointF kCursorMoved(400, 360);

int rgbDistance(QRgb a, QRgb b)
{
    return std::abs(qRed(a) - qRed(b))
        + std::abs(qGreen(a) - qGreen(b))
        + std::abs(qBlue(a) - qBlue(b));
}

int regionDiffCount(const QImage &a, const QImage &b, const QRegion &region, int minDistance,
                    QPoint *firstMismatch = nullptr)
{
    if (a.size() != b.size())
        return -1;
    int count = 0;
    for (const QRect &part : region) {
        const QRect bounded = part.intersected(a.rect());
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            for (int x = bounded.left(); x <= bounded.right(); ++x) {
                if (rgbDistance(a.pixel(x, y), b.pixel(x, y)) > minDistance) {
                    if (count == 0 && firstMismatch)
                        *firstMismatch = QPoint(x, y);
                    ++count;
                }
            }
        }
    }
    return count;
}

int regionDiffCount(const QImage &a, const QImage &b, const QRect &region, int minDistance,
                    QPoint *firstMismatch = nullptr)
{
    return regionDiffCount(a, b, QRegion(region), minDistance, firstMismatch);
}

struct FrameSnap {
    bool valid = false;
    bool full = false;
    QRegion region;
    QImage buffer;
    QImage grab;
    QList<WSGDamageDebug::Entry> entries;
    qint64 nowMs = 0;
};

QImage toArgb32(const QImage &image)
{
    return image.convertToFormat(QImage::Format_ARGB32);
}

} // namespace

class WSGDamageDebugVisualTest : public QObject
{
    Q_OBJECT

public:
    static void setGlobals(WOutputRenderWindow *window, WOutputViewport *viewport,
                           QQuickItem *scene, VisualHelper *helper)
    {
        m_window = window;
        m_viewport = viewport;
        m_scene = scene;
        m_helper = helper;
    }

private:
    static inline WOutputRenderWindow *m_window = nullptr;
    static inline WOutputViewport *m_viewport = nullptr;
    static inline QQuickItem *m_scene = nullptr;
    static inline VisualHelper *m_helper = nullptr;

    enum class CursorPath {
        Hardware,
        SoftwareLayer,
        SoftwareInline,
    };

    QQuickItem *named(const char *name) const
    {
        return m_scene ? m_scene->findChild<QQuickItem *>(QLatin1String(name)) : nullptr;
    }

    QQuickItem *cursorItem() const
    {
        if (auto *item = m_window->findChild<QQuickItem *>(QStringLiteral("outputCursor")))
            return item;
        auto *outputItem = m_window->findChild<WOutputItem *>();
        if (!outputItem || outputItem->cursorItems().isEmpty())
            return nullptr;
        return outputItem->cursorItems().constFirst();
    }

    WOutputLayer *cursorLayer() const
    {
        QQuickItem *item = cursorItem();
        if (!item)
            return nullptr;
        return qobject_cast<WOutputLayer *>(qmlAttachedPropertiesObject<WOutputLayer>(item, false));
    }

    QString cursorPathDebug() const
    {
        auto *item = cursorItem();
        auto *layer = cursorLayer();
        auto *output = m_viewport ? m_viewport->output() : nullptr;
        const bool onHw = layer && m_viewport && layer->inOutputsByHardware().contains(m_viewport);
        return QStringLiteral("visible=%1 simulateHw=%2 enabled=%3 keepLayer=%4 inHardware=%5 forceSoftware=%6 disableHwLayers=%7")
            .arg(item ? item->isVisible() : false)
            .arg(item ? item->property("simulateHardwareCursor").toBool() : false)
            .arg(layer ? layer->enabled() : false)
            .arg(layer ? layer->keepLayer() : false)
            .arg(onHw)
            .arg(output ? output->forceSoftwareCursor() : false)
            .arg(m_viewport ? m_viewport->disableHardwareLayers() : false);
    }

    bool cursorPathMatches(CursorPath path) const
    {
        auto *item = cursorItem();
        auto *layer = cursorLayer();
        if (!item || !layer || !m_viewport)
            return false;
        const bool simulateHw = item->property("simulateHardwareCursor").toBool();
        const bool onHw = layer->inOutputsByHardware().contains(m_viewport);
        switch (path) {
        case CursorPath::Hardware:
            return simulateHw && !item->isVisible();
        case CursorPath::SoftwareLayer:
            return !simulateHw && item->isVisible() && layer->enabled() && layer->keepLayer() && !onHw
                && m_viewport->layers().contains(layer);
        case CursorPath::SoftwareInline:
            return !simulateHw && item->isVisible() && !layer->enabled() && !onHw;
        }
        return false;
    }

    bool configureCursorPath(CursorPath path)
    {
        auto *output = m_viewport ? m_viewport->output() : nullptr;
        auto *item = cursorItem();
        auto *layer = cursorLayer();
        if (!output || !item || !layer)
            return false;

        item->setProperty("simulateHardwareCursor", path == CursorPath::Hardware);

        switch (path) {
        case CursorPath::Hardware:
            // Cursor is not drawn into the output buffer (hardware plane).
            output->setForceSoftwareCursor(false);
            m_viewport->setDisableHardwareLayers(false);
            layer->setKeepLayer(true);
            layer->setEnabled(true);
            break;
        case CursorPath::SoftwareLayer:
            output->setForceSoftwareCursor(true);
            m_viewport->setDisableHardwareLayers(true);
            layer->setKeepLayer(true);
            layer->setEnabled(true);
            break;
        case CursorPath::SoftwareInline:
            output->setForceSoftwareCursor(true);
            m_viewport->setDisableHardwareLayers(true);
            layer->setEnabled(false);
            break;
        }

        parkCursor();
        for (int i = 0; i < 8; ++i) {
            if (!captureOutput(false).valid)
                return false;
            if (cursorPathMatches(path))
                return true;
        }
        return cursorPathMatches(path);
    }

    void restoreCursorDefaults()
    {
        if (auto *output = m_viewport ? m_viewport->output() : nullptr)
            output->setForceSoftwareCursor(false);
        if (m_viewport)
            m_viewport->setDisableHardwareLayers(false);
        if (auto *item = cursorItem())
            item->setProperty("simulateHardwareCursor", false);
        if (auto *layer = cursorLayer()) {
            layer->setKeepLayer(true);
            layer->setEnabled(true);
        }
    }

    void parkCursor()
    {
        auto *cursor = m_helper->cursor();
        cursor->setVisible(true);
        if (cursor->position() == kCursorHome)
            cursor->setPosition(kCursorHome + QPointF(1, 1));
        cursor->setPosition(kCursorHome);
    }

    QRegion cursorIgnoreRegion() const
    {
        QRegion region;
        if (auto *item = cursorItem()) {
            const QRect rect = outputRect(item);
            if (!rect.isEmpty())
                region += rect.adjusted(-kCursorPad, -kCursorPad, kCursorPad, kCursorPad);
        }
        // grabToImage is the QML scene without the cursor sibling, so dump vs
        // grab must ignore every slot this test parks/moves the cursor through.
        const QSize slot(48, 48);
        region += QRect(kCursorHome.toPoint(), slot).adjusted(-kCursorPad, -kCursorPad, kCursorPad, kCursorPad);
        region += QRect(kCursorMoved.toPoint(), slot).adjusted(-kCursorPad, -kCursorPad, kCursorPad, kCursorPad);
        return region;
    }

    QRect outputRect(QQuickItem *item) const
    {
        if (!item || !m_viewport)
            return {};
        return m_viewport->mapToOutput(item, item->boundingRect()).toAlignedRect();
    }

    QRect sceneRect(QQuickItem *item) const
    {
        if (!item)
            return {};
        return item->mapRectToScene(item->boundingRect()).toAlignedRect();
    }

    QQuickItem *popupPanel() const
    {
        return m_window->findChild<QQuickItem *>(QStringLiteral("popupPanel"));
    }

    void restoreViewportTransform()
    {
        if (!m_viewport)
            return;
        m_viewport->setRotation(0);
        m_viewport->setScale(1);
        m_viewport->rotateOutput(WOutput::Normal);
        m_viewport->setOutputScale(1.f);
    }

    bool applyViewportTransform(qreal rotation, qreal scale, WOutput::Transform transform,
                                float outputScale = 1.f)
    {
        restoreViewportTransform();
        m_viewport->setRotation(rotation);
        m_viewport->setScale(scale);
        m_viewport->rotateOutput(transform);
        m_viewport->setOutputScale(outputScale);
        for (int i = 0; i < 4; ++i) {
            if (!captureOutput(false).valid)
                return false;
        }
        return true;
    }

    bool expectItemMoveIsPartial(QQuickItem *item, qreal dx, qreal dy, const char *what)
    {
        if (!item)
            return false;
        const QRect oldRect = sceneRect(item);
        item->setX(item->x() + dx);
        item->setY(item->y() + dy);
        const QRect now = sceneRect(item);

        const FrameSnap snap = captureOrSkip();
        const int pad = item == named("blurPanel") ? blurPad() : shadowPad();
        const bool ok = expectRegionCovers(snap, oldRect.adjusted(-pad, -pad, pad, pad),
                                           qPrintable(QStringLiteral("%1 old").arg(QLatin1String(what))))
            && expectRegionCovers(snap, now.adjusted(-pad, -pad, pad, pad),
                                  qPrintable(QStringLiteral("%1 new").arg(QLatin1String(what))))
            && expectRegionAvoids(snap, sceneRect(named("sentinel")),
                                  qPrintable(QStringLiteral("%1 sentinel").arg(QLatin1String(what))))
            && expectRegionNotFullscreen(snap, what);
        return ok;
    }

    bool expectTargetMoveIsPartial(const char *what)
    {
        return expectItemMoveIsPartial(named("target"), 55, 20, what);
    }

    int shadowPad() const
    {
        return m_scene ? m_scene->property("shadowPad").toInt() : 28;
    }

    int blurPad() const
    {
        return m_scene ? m_scene->property("blurPad").toInt() : 20;
    }

    FrameSnap captureOutput(bool grabScene, bool forceViewportRender = true)
    {
        FrameSnap snap;
        WBufferDumper::DumpResult result = WBufferDumper::DumpResult::InvalidBuffer;
        wlr_buffer *pendingDump = nullptr;
        bool gotBatchFlush = false;

        const QMetaObject::Connection conn = connect(
            m_window, &QQuickWindow::afterRendering, this, [&] {
                auto *br = WOutputViewportPrivate::get(m_viewport)->bufferRenderer;
                if (!br)
                    return;
                pendingDump = br->currentBuffer();
                if (!pendingDump)
                    pendingDump = br->lastBuffer();
                // Batch renderer is cleared in endRender(); snapshot it here.
                if (auto *batch = dynamic_cast<WSGBatchRenderer::Renderer *>(br->currentRenderer())) {
                    snap.full = batch->flushRegionIsFull();
                    snap.region = batch->flushRegion();
                    gotBatchFlush = true;
                    if (const auto *debug = batch->damageDebug()) {
                        snap.entries = debug->entries();
                        snap.nowMs = debug->currentTimeMs();
                    }
                }
            },
            Qt::DirectConnection);
        if (forceViewportRender)
            m_viewport->render(true);
        else
            m_window->render();
        disconnect(conn);

        // afterRendering runs before QQuickRenderControl::endFrame(). Vulkan
        // submits the pass there, so the dump must wait until render() returns.
        auto *br = WOutputViewportPrivate::get(m_viewport)->bufferRenderer;
        if (br && !gotBatchFlush) {
            snap.full = br->lastFlushIsFull();
            snap.region = br->lastFlushRegion();
            if (const auto *debug = br->damageDebugOverlay()) {
                snap.entries = debug->entries();
                snap.nowMs = debug->currentTimeMs();
            }
        }
        wlr_buffer *buffer = br ? br->lastBuffer() : nullptr;
        if (!buffer)
            buffer = pendingDump;
        if (buffer)
            result = WBufferDumper::dumpBufferToImage(buffer, m_helper->renderer(), snap.buffer);

        snap.valid = result == WBufferDumper::DumpResult::Success && !snap.buffer.isNull();
        if (snap.valid)
            snap.buffer = toArgb32(snap.buffer);
        else
            qWarning("dumpBufferToImage failed: %s",
                     qPrintable(WBufferDumper::dumpResultToString(result)));

        if (grabScene && snap.valid) {
            // grabToImage refs the item as an effect source and would dirty
            // this frame's flushRegion, so it runs on a follow-up frame.
            const auto previous = WSGDamageDebug::mode();
            WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);
            const auto grab = m_scene->grabToImage();
            if (!grab) {
                qWarning("grabToImage could not be initiated");
            } else {
                QSignalSpy grabSpy(grab.data(), &QQuickItemGrabResult::ready);
                m_viewport->render(true);
                if (grabSpy.count() == 0)
                    grabSpy.wait(5000);
                snap.grab = toArgb32(grab->image());
                if (snap.grab.isNull())
                    qWarning("grabToImage did not produce an image");
            }
            WSGDamageDebug::setMode(previous);
        }
        return snap;
    }

    QImage expectedOverlayImage(const QImage &clean, const FrameSnap &snap) const
    {
        QImage painted = clean;
        if (painted.size() != snap.buffer.size())
            painted = painted.scaled(snap.buffer.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        QPainter painter(&painted);
        WSGDamageDebug::paint(&painter, QTransform(), snap.entries, snap.nowMs);
        painter.end();
        return toArgb32(painted);
    }

    bool waitUntilNoClientSurface()
    {
        if (!m_window || !m_helper)
            return true;

        QElapsedTimer timer;
        timer.start();
        while (m_window->findChild<QQuickItem *>(QStringLiteral("clientSurface"))
               && timer.elapsed() < 2000) {
            m_helper->dispatchWaylandEvents();
            QTest::qWait(0);
        }
        if (m_window->findChild<QQuickItem *>(QStringLiteral("clientSurface")))
            return false;

        // The xdg item is a sibling of DamageScene, so leftover pixels stay in
        // the dumped output until unmap damage is actually redrawn.
        for (int i = 0; i < 3; ++i) {
            if (!captureOutput(false).valid)
                return false;
        }
        return true;
    }

    bool resetPlainScene()
    {
        WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);
        restoreViewportTransform();
        parkCursor();
        if (!QMetaObject::invokeMethod(m_scene, "resetScene", Qt::DirectConnection))
            return false;
        for (int i = 0; i < 3; ++i) {
            if (!captureOutput(false).valid)
                return false;
        }
        return true;
    }

    FrameSnap captureOrSkip(bool grabScene = false, bool forceViewportRender = true)
    {
        FrameSnap snap = captureOutput(grabScene, forceViewportRender);
        if (!snap.valid)
            QTest::qSkip("Could not dump the committed output buffer to QImage", __FILE__, __LINE__);
        return snap;
    }

    bool usesTrackedPartialDamage() const
    {
        // QSGSoftwareRenderer / pixman does not use WSGDamageTracker. Its
        // flushRegion is often the whole paint device, so partial-damage
        // assertions are RHI-only. Overlay paint and rerender still run.
        return WRenderHelper::getGraphicsApi() != QSGRendererInterface::Software;
    }

    bool vulkanSkipsMultiEffectGrab() const
    {
        // grabToImage of live MultiEffect (blur / drop shadow) is unreliable
        // on Vulkan after those items move. Region checks still run.
        return WRenderHelper::getGraphicsApi() == QSGRendererInterface::Vulkan;
    }

    bool expectRegionCovers(const FrameSnap &snap, const QRect &rect, const char *what)
    {
        if (!usesTrackedPartialDamage())
            return true;
        const QString prefix = QLatin1String(what);
        if (!QTest::qVerify(!snap.full,
                            "!snap.full",
                            qPrintable(QStringLiteral("%1: expected partial damage, got full").arg(prefix)),
                            __FILE__, __LINE__)) {
            return false;
        }
        QRect probe = rect.adjusted(1, 1, -1, -1);
        if (probe.isEmpty())
            probe = rect;
        return QTest::qVerify(snap.region.intersects(probe) || snap.region.contains(rect),
                              "region covers",
                              qPrintable(QStringLiteral("%1: damage %2 missed %3,%4 %5x%6")
                                             .arg(prefix, WSGDamageDebug::describe(snap.region, snap.full))
                                             .arg(rect.x())
                                             .arg(rect.y())
                                             .arg(rect.width())
                                             .arg(rect.height())),
                              __FILE__, __LINE__);
    }

    bool expectRegionAvoids(const FrameSnap &snap, const QRect &rect, const char *what)
    {
        if (!usesTrackedPartialDamage())
            return true;
        const QString prefix = QLatin1String(what);
        if (!QTest::qVerify(!snap.full,
                            "!snap.full",
                            qPrintable(QStringLiteral("%1: expected partial damage, got full").arg(prefix)),
                            __FILE__, __LINE__)) {
            return false;
        }
        return QTest::qVerify(!snap.region.intersects(rect),
                              "region avoids",
                              qPrintable(QStringLiteral("%1: damage %2 leaked into %3,%4 %5x%6")
                                             .arg(prefix, WSGDamageDebug::describe(snap.region, snap.full))
                                             .arg(rect.x())
                                             .arg(rect.y())
                                             .arg(rect.width())
                                             .arg(rect.height())),
                              __FILE__, __LINE__);
    }

    bool expectRegionNotFullscreen(const FrameSnap &snap, const char *what)
    {
        if (!usesTrackedPartialDamage())
            return true;
        const QString prefix = QLatin1String(what);
        if (!QTest::qVerify(!snap.full,
                            "!snap.full",
                            qPrintable(QStringLiteral("%1: expected partial damage, got full (fullscreen red)")
                                           .arg(prefix)),
                            __FILE__, __LINE__)) {
            return false;
        }
        const QRect bounds = snap.region.boundingRect();
        const int area = bounds.width() * bounds.height();
        constexpr int kMaxArea = 800 * 480 / 4;
        return QTest::qVerify(area <= kMaxArea,
                              "region not fullscreen",
                              qPrintable(QStringLiteral("%1: damage %2 covers %3 px (max %4)")
                                             .arg(prefix, WSGDamageDebug::describe(snap.region, snap.full))
                                             .arg(area)
                                             .arg(kMaxArea)),
                              __FILE__, __LINE__);
    }

    bool expectImagesMatch(const QImage &actual, const QImage &expected, const char *what,
                           bool ignoreCursor = true, const QRegion &limit = {})
    {
        QPoint first;
        QRegion compared = limit.isEmpty() ? QRegion(expected.rect()) : limit;
        if (ignoreCursor)
            compared -= cursorIgnoreRegion();
        int mismatches = regionDiffCount(actual, expected, compared, kRgbTolerance, &first);
        if (mismatches > kMaxMismatchPixels) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
            const QImage flipped = expected.flipped(Qt::Vertical);
#else
            const QImage flipped = expected.mirrored(false, true);
#endif
            QPoint flippedFirst;
            const int flippedMismatches =
                regionDiffCount(actual, flipped, compared, kRgbTolerance, &flippedFirst);
            if (flippedMismatches < mismatches) {
                mismatches = flippedMismatches;
                first = flippedFirst;
            }
        }
        if (mismatches > kMaxMismatchPixels) {
            const QString dir = QDir::temp().filePath(QStringLiteral("wsg-damage-fail"));
            QDir().mkpath(dir);
            const QString stamp = QString::fromLatin1(what).replace(QLatin1Char(' '), QLatin1Char('_'));
            actual.save(dir + QLatin1Char('/') + stamp + QStringLiteral("-actual.png"));
            expected.save(dir + QLatin1Char('/') + stamp + QStringLiteral("-expected.png"));
        }
        return QTest::qVerify(mismatches >= 0 && mismatches <= kMaxMismatchPixels,
                              "images match",
                              qPrintable(QStringLiteral("%1: mismatch %2 pixels, first %3,%4")
                                             .arg(QLatin1String(what))
                                             .arg(mismatches)
                                             .arg(first.x())
                                             .arg(first.y())),
                              __FILE__, __LINE__);
    }

    QRegion overlayMask(const FrameSnap &snap) const
    {
        QRegion region;
        for (const auto &entry : snap.entries) {
            if (entry.full)
                return QRegion(snap.buffer.rect());
            region += entry.region;
        }
        QRegion inflated;
        for (const QRect &rect : region)
            inflated += rect.adjusted(-kOverlayPad, -kOverlayPad, kOverlayPad, kOverlayPad);
        return inflated;
    }

    bool expectNoOverlayResidue(const FrameSnap &snap, const char *what)
    {
        const QString prefix = QLatin1String(what);
        if (!QTest::qVerify(!snap.grab.isNull(),
                            "!snap.grab.isNull()",
                            qPrintable(QStringLiteral("%1: grabToImage failed").arg(prefix)),
                            __FILE__, __LINE__)) {
            return false;
        }
        const QRegion outside = QRegion(snap.buffer.rect()) - overlayMask(snap) - cursorIgnoreRegion();
        int mismatches = 0;
        QPoint first;
        for (const QRect &rect : outside) {
            QPoint rectFirst;
            const int n = regionDiffCount(snap.buffer, snap.grab, rect, kRgbTolerance, &rectFirst);
            if (n < 0)
                return false;
            if (n > 0 && mismatches == 0)
                first = rectFirst;
            mismatches += n;
        }
        if (mismatches != 0) {
            const QString dir = QDir::temp().filePath(QStringLiteral("wsg-damage-fail"));
            QDir().mkpath(dir);
            const QString stamp = QString(prefix).replace(QLatin1Char(' '), QLatin1Char('_'));
            snap.buffer.save(dir + QLatin1Char('/') + stamp + QStringLiteral("-residue-actual.png"));
            snap.grab.save(dir + QLatin1Char('/') + stamp + QStringLiteral("-residue-grab.png"));
        }
        return QTest::qVerify(mismatches == 0,
                              "no overlay residue",
                              qPrintable(QStringLiteral("%1: leftover overlay %2 pixels outside damage, first %3,%4")
                                             .arg(prefix)
                                             .arg(mismatches)
                                             .arg(first.x())
                                             .arg(first.y())),
                              __FILE__, __LINE__);
    }

    bool expectOverlayMatchesGrab(const FrameSnap &snap, const char *what, bool ignoreCursor = true)
    {
        const QString prefix = QLatin1String(what);
        if (!QTest::qVerify(snap.valid, "snap.valid", qPrintable(prefix), __FILE__, __LINE__))
            return false;
        if (!QTest::qVerify(!snap.grab.isNull(),
                            "!snap.grab.isNull()",
                            qPrintable(QStringLiteral("%1: grabToImage failed").arg(prefix)),
                            __FILE__, __LINE__)) {
            return false;
        }
        if (!expectNoOverlayResidue(snap, what))
            return false;
        return expectImagesMatch(snap.buffer, expectedOverlayImage(snap.grab, snap), what, ignoreCursor);
    }

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void idle_twoCapturesMatch();
    void highlight_moveRegionAndOverlay();
    void highlight_blurMoveRegionAndOverlay();
    void frosted_behindMoveMatchesRerender();
    void highlight_hardwareCursorMoveRegion();
    void highlight_softwareLayerCursorMoveRegion();
    void highlight_softwareInlineCursorMoveRegion();
    void highlight_fadeFramesMatchPainter();
    void highlight_idleCompositorErasesOverlay();
    void highlight_popupOpenRegion();
    void highlight_popupCloseRegion();
    void highlight_viewportRotation_data();
    void highlight_viewportRotation();
    void highlight_viewportScale_data();
    void highlight_viewportScale();
    void highlight_viewportRotatedScaledItemMoves();
    void client_commitDamageRegion();
    void rerender_hasNoOverlay();
};

void WSGDamageDebugVisualTest::initTestCase()
{
    QVERIFY(m_window);
    QVERIFY(m_viewport);
    QVERIFY(m_scene);
    QVERIFY(named("target"));
    QVERIFY(named("sentinel"));
    QVERIFY(named("blurPanel"));
    qInfo("WLR_RENDERER=%s graphicsApi=%d",
          qgetenv("WLR_RENDERER").constData(),
          int(WRenderHelper::getGraphicsApi()));
    if (!usesTrackedPartialDamage())
        qInfo("software/pixman: overlay and rerender checks only (no WSGDamageTracker)");
    if (vulkanSkipsMultiEffectGrab())
        qInfo("Vulkan: skipping MultiEffect (blur/shadow) grabToImage comparisons");

    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);
    parkCursor();
    for (int i = 0; i < 4; ++i)
        QVERIFY2(captureOutput(false).valid, "initial output dump failed");

    const FrameSnap idle = captureOrSkip();
    const QRect target = outputRect(named("target"));
    const QRect sentinel = outputRect(named("sentinel"));
    QVERIFY(idle.buffer.rect().contains(target));
    QVERIFY(idle.buffer.rect().contains(sentinel));
}

void WSGDamageDebugVisualTest::cleanup()
{
    restoreCursorDefaults();
    restoreViewportTransform();
    parkCursor();
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);
    if (m_window && m_window->findChild<QQuickItem *>(QStringLiteral("clientSurface")))
        QVERIFY(waitUntilNoClientSurface());
    for (int i = 0; i < 4; ++i) {
        QVERIFY(captureOutput(false).valid);
        auto *br = WOutputViewportPrivate::get(m_viewport)->bufferRenderer;
        if (!br || !br->damageDebugNeedsFrame())
            break;
    }
}

void WSGDamageDebugVisualTest::idle_twoCapturesMatch()
{
    QVERIFY(resetPlainScene());
    const FrameSnap a = captureOrSkip(true);
    const FrameSnap b = captureOrSkip(true);
    QCOMPARE(regionDiffCount(a.buffer, b.buffer, a.buffer.rect(), kRgbTolerance), 0);
    QVERIFY2(!a.grab.isNull() && !b.grab.isNull(), "idle grabToImage failed");
    QVERIFY(expectImagesMatch(a.buffer, a.grab, "idle dump vs grab"));
    QVERIFY(expectImagesMatch(b.buffer, b.grab, "idle dump vs grab (2)"));
}

void WSGDamageDebugVisualTest::highlight_moveRegionAndOverlay()
{
    QVERIFY(resetPlainScene());
    auto *target = named("target");
    const QRect oldRect = outputRect(target);
    target->setX(target->x() + 70);
    target->setY(target->y() + 30);
    const QRect now = outputRect(target);

    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    const FrameSnap snap = captureOrSkip(true);

    const int pad = shadowPad();
    QVERIFY(expectRegionCovers(snap, oldRect.adjusted(-pad, -pad, pad, pad), "move old"));
    QVERIFY(expectRegionCovers(snap, now.adjusted(-pad, -pad, pad, pad), "move new"));
    QVERIFY(expectRegionAvoids(snap, outputRect(named("sentinel")), "move sentinel"));
    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect (shadow) grabToImage is skipped");
    QVERIFY(expectOverlayMatchesGrab(snap, "move overlay"));
}

void WSGDamageDebugVisualTest::highlight_blurMoveRegionAndOverlay()
{
    QVERIFY(resetPlainScene());
    auto *panel = named("blurPanel");
    const QRect oldRect = outputRect(panel);
    panel->setX(panel->x() + 40);
    panel->setY(panel->y() + 20);
    const QRect now = outputRect(panel);

    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    const FrameSnap snap = captureOrSkip(true);

    const int pad = blurPad();
    QVERIFY(expectRegionCovers(snap, oldRect.adjusted(-pad, -pad, pad, pad), "blur old"));
    QVERIFY(expectRegionCovers(snap, now.adjusted(-pad, -pad, pad, pad), "blur new"));
    QVERIFY(expectRegionAvoids(snap, outputRect(named("sentinel")), "blur sentinel"));
    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect (blur) grabToImage is skipped");
    QVERIFY(expectOverlayMatchesGrab(snap, "blur overlay"));
}

void WSGDamageDebugVisualTest::frosted_behindMoveMatchesRerender()
{
    QVERIFY(resetPlainScene());
    auto *glass = named("frostGlass");
    auto *behind = named("frostBehind");
    QVERIFY(glass);
    QVERIFY(behind);
    glass->setVisible(true);
    for (int i = 0; i < 4; ++i)
        QVERIFY(captureOutput(false).valid);

    behind->setX(behind->x() + 45);
    behind->setY(behind->y() + 25);

    const FrameSnap partial = captureOrSkip(false);
    QVERIFY(expectRegionCovers(partial, outputRect(glass), "frost glass"));
    QVERIFY(expectRegionAvoids(partial, outputRect(named("sentinel")), "frost sentinel"));

    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Rerender);
    const FrameSnap full = captureOrSkip(false);
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);

    const QRect glassRect = outputRect(glass);
    QVERIFY2(!glassRect.isEmpty(), "frost glass has no output rect");
    QVERIFY(expectImagesMatch(partial.buffer, full.buffer, "frosted partial vs rerender",
                              true, QRegion(glassRect)));
}

void WSGDamageDebugVisualTest::highlight_hardwareCursorMoveRegion()
{
    QVERIFY(resetPlainScene());
    QVERIFY2(configureCursorPath(CursorPath::Hardware),
             qPrintable(QStringLiteral("hardware cursor hide did not apply: %1").arg(cursorPathDebug())));
    QVERIFY2(!cursorItem()->isVisible(),
             "hardware cursor must hide the QML cursor item (not drawn into the output)");

    auto *cursor = m_helper->cursor();
    cursor->setVisible(true);
    cursor->setPosition(kCursorHome);
    for (int i = 0; i < 3; ++i)
        QVERIFY(captureOutput(false).valid);
    QVERIFY2(!cursorItem()->isVisible(),
             "QML cursor became visible while simulating the hardware plane");

    cursor->setPosition(kCursorMoved);
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    const FrameSnap snap = captureOrSkip(true);

    QVERIFY2(!cursorItem()->isVisible(),
             qPrintable(QStringLiteral("hardware cursor simulation lost after move: %1").arg(cursorPathDebug())));
    QVERIFY(expectRegionNotFullscreen(snap, "hardware cursor move"));
    QVERIFY(expectRegionAvoids(snap, outputRect(named("sentinel")), "hardware cursor sentinel"));
    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect grabToImage is skipped");
    QVERIFY(expectOverlayMatchesGrab(snap, "hardware cursor overlay", false));

    cursor->setPosition(kCursorHome);
    QVERIFY(captureOutput(false).valid);
}

void WSGDamageDebugVisualTest::highlight_softwareLayerCursorMoveRegion()
{
    QVERIFY(resetPlainScene());
    QVERIFY2(configureCursorPath(CursorPath::SoftwareLayer),
             qPrintable(QStringLiteral("software cursor layer did not enable: %1").arg(cursorPathDebug())));
    QVERIFY2(!cursorLayer()->inOutputsByHardware().contains(m_viewport),
             "software layer cursor must not be in hardware");
    QVERIFY2(m_viewport->layers().contains(cursorLayer()),
             "independent software cursor must stay an OutputLayer");

    auto *cursor = m_helper->cursor();
    cursor->setVisible(true);
    cursor->setPosition(kCursorHome);
    for (int i = 0; i < 3; ++i)
        QVERIFY(captureOutput(false).valid);

    cursor->setPosition(kCursorMoved);
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    const FrameSnap snap = captureOrSkip(true);

    QVERIFY2(cursorPathMatches(CursorPath::SoftwareLayer),
             qPrintable(QStringLiteral("software cursor layer dropped after move: %1").arg(cursorPathDebug())));
    QVERIFY(expectRegionNotFullscreen(snap, "software layer cursor move"));
    QVERIFY(expectRegionAvoids(snap, outputRect(named("sentinel")), "software layer cursor sentinel"));
    // The cursor is a separate OutputLayer, so grabToImage(m_scene) and the
    // primary output dump do not share overlay pixels the way inline items do.

    cursor->setPosition(kCursorHome);
    QVERIFY(captureOutput(false).valid);
}

void WSGDamageDebugVisualTest::highlight_softwareInlineCursorMoveRegion()
{
    QVERIFY(resetPlainScene());
    QVERIFY2(configureCursorPath(CursorPath::SoftwareInline),
             qPrintable(QStringLiteral("inline software cursor did not enable: %1").arg(cursorPathDebug())));
    QVERIFY2(!cursorLayer()->enabled(), "inline software cursor must disable OutputLayer");

    auto *item = cursorItem();
    QVERIFY2(item, "cursor item was not created");

    auto *cursor = m_helper->cursor();
    cursor->setVisible(true);
    cursor->setPosition(kCursorHome);
    for (int i = 0; i < 3; ++i)
        QVERIFY(captureOutput(false).valid);

    const QRect oldRect = outputRect(item);
    QVERIFY2(!oldRect.isEmpty(), "inline software cursor has empty bounds at home");
    cursor->setPosition(kCursorMoved);
    const QRect now = outputRect(item);
    QVERIFY2(oldRect != now, "WCursor::setPosition did not move the cursor item");

    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    const FrameSnap snap = captureOrSkip(true);

    QVERIFY(expectRegionCovers(snap, oldRect.adjusted(-kCursorPad, -kCursorPad, kCursorPad, kCursorPad),
                               "inline software cursor old"));
    QVERIFY(expectRegionCovers(snap, now.adjusted(-kCursorPad, -kCursorPad, kCursorPad, kCursorPad),
                               "inline software cursor new"));
    QVERIFY(expectRegionAvoids(snap, outputRect(named("sentinel")), "inline software cursor sentinel"));
    QVERIFY(expectRegionNotFullscreen(snap, "inline software cursor move"));
    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect grabToImage is skipped");
    QVERIFY(expectOverlayMatchesGrab(snap, "inline software cursor overlay"));

    cursor->setPosition(kCursorHome);
    QVERIFY(captureOutput(false).valid);
}

void WSGDamageDebugVisualTest::highlight_fadeFramesMatchPainter()
{
    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect (shadow) grabToImage is skipped");
    QVERIFY(resetPlainScene());
    auto *target = named("target");
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    target->setX(target->x() + 70);
    QVERIFY(captureOutput(false).valid);

    for (int i = 0; i < 5; ++i) {
        QTest::qWait(40);
        const FrameSnap snap = captureOrSkip(true);
        QVERIFY(expectOverlayMatchesGrab(snap, "fade frame"));
    }

    QTest::qWait(WSGDamageDebug::fadeOutMs);
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);
    for (int i = 0; i < 3; ++i)
        QVERIFY(captureOutput(false).valid);
    const FrameSnap faded = captureOrSkip(true);
    QVERIFY2(!faded.grab.isNull(), "fade grabToImage failed");
    QVERIFY(expectImagesMatch(faded.buffer, faded.grab, "fade left overlay residue"));
}

void WSGDamageDebugVisualTest::highlight_idleCompositorErasesOverlay()
{
    if (!usesTrackedPartialDamage())
        QSKIP("idle overlay residue is an RHI PreserveColorContents swapchain path");

    // viewport->render(true) sets forceRender, so highlight_fadeFramesMatchPainter
    // never hits doRenderOutputs' "skip if !contentIsDirty" path. Overlay fade
    // only scheduled a frame; the compositor then skipped the redraw and left
    // red rects in PreserveColorContents swapchain images.
    QVERIFY(resetPlainScene());
    auto *target = named("target");
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    target->setX(target->x() + 70);

    // Do not grab here: grabToImage turns highlight off and would expire
    // the fade entries before the idle compositor path runs.
    const FrameSnap painted = captureOrSkip(false);
    QVERIFY2(!painted.entries.isEmpty(), "highlight did not record overlay entries");

    auto *br = WOutputViewportPrivate::get(m_viewport)->bufferRenderer;
    QVERIFY2(br && br->damageDebugNeedsFrame(),
             "overlay fade must request another compositor frame");

    QTest::qWait(WSGDamageDebug::fadeOutMs + 50);

    FrameSnap idle;
    for (int i = 0; i < 4; ++i) {
        idle = captureOutput(false, false);
        QVERIFY2(idle.valid, "compositor idle frame could not dump the output buffer");
    }
    idle = captureOrSkip(true, false);
    QVERIFY2(!idle.grab.isNull(), "idle compositor grabToImage failed");
    QVERIFY2(!br->damageDebugNeedsFrame(),
             "overlay entries should expire after idle compositor frames");

    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect grabToImage is skipped");

    FrameSnap paintedWithGrab = painted;
    paintedWithGrab.grab = idle.grab;
    QVERIFY(expectOverlayMatchesGrab(paintedWithGrab, "overlay before idle compositor"));
    QVERIFY(expectImagesMatch(idle.buffer, idle.grab, "idle compositor left overlay residue"));

    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);
}

void WSGDamageDebugVisualTest::highlight_popupOpenRegion()
{
    QVERIFY(resetPlainScene());
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    QVERIFY(QMetaObject::invokeMethod(m_scene, "openPopup", Qt::DirectConnection));
    const FrameSnap snap = captureOrSkip();
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);

    auto *panel = popupPanel();
    QVERIFY2(panel, "popupPanel was not in the scene");
    QVERIFY(panel->isVisible());
    QVERIFY(expectRegionCovers(snap, sceneRect(panel), "popup open"));
    QVERIFY(expectRegionAvoids(snap, sceneRect(named("sentinel")), "popup open sentinel"));
    QVERIFY(expectRegionNotFullscreen(snap, "popup open"));
    // grabToImage(m_scene) does not include QQuickOverlay, so dump vs grab
    // cannot verify the popup pixels. Region checks above are the contract.
}

void WSGDamageDebugVisualTest::highlight_popupCloseRegion()
{
    QVERIFY(resetPlainScene());
    QVERIFY(QMetaObject::invokeMethod(m_scene, "openPopup", Qt::DirectConnection));
    QVERIFY(captureOutput(false).valid);

    auto *panel = popupPanel();
    QVERIFY2(panel, "popupPanel was not in the scene");
    const QRect oldRect = sceneRect(panel);

    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Highlight);
    QVERIFY(QMetaObject::invokeMethod(m_scene, "closePopup", Qt::DirectConnection));
    const FrameSnap snap = captureOrSkip(true);
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);

    QVERIFY(expectRegionCovers(snap, oldRect, "popup close"));
    QVERIFY(expectRegionAvoids(snap, sceneRect(named("sentinel")), "popup close sentinel"));
    QVERIFY(expectRegionNotFullscreen(snap, "popup close"));
    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect grabToImage is skipped");
    QVERIFY(expectOverlayMatchesGrab(snap, "popup close overlay"));
}

void WSGDamageDebugVisualTest::highlight_viewportRotation_data()
{
    QTest::addColumn<qreal>("itemRotation");
    QTest::addColumn<int>("transform");
    QTest::newRow("0") << 0. << int(WOutput::Normal);
    QTest::newRow("90") << 90. << int(WOutput::R90);
    QTest::newRow("180") << 180. << int(WOutput::R180);
    QTest::newRow("270") << -90. << int(WOutput::R270);
}

void WSGDamageDebugVisualTest::highlight_viewportRotation()
{
    QFETCH(qreal, itemRotation);
    QFETCH(int, transform);

    QVERIFY(resetPlainScene());
    QVERIFY2(applyViewportTransform(itemRotation, 1.0, WOutput::Transform(transform)),
             "output dump failed after viewport rotation");
    QVERIFY(expectTargetMoveIsPartial("viewport rotation target"));
    QVERIFY(captureOutput(false).valid);
    QVERIFY(expectItemMoveIsPartial(named("blurPanel"), -35, 25, "viewport rotation blur"));
}

void WSGDamageDebugVisualTest::highlight_viewportScale_data()
{
    QTest::addColumn<qreal>("itemScale");
    QTest::addColumn<float>("outputScale");
    QTest::newRow("item-0.8") << 0.8 << 1.f;
    QTest::newRow("item-1.25") << 1.25 << 1.f;
    QTest::newRow("item-1.5") << 1.5 << 1.f;
    QTest::newRow("output-1.5") << 1.0 << 1.5f;
}

void WSGDamageDebugVisualTest::highlight_viewportScale()
{
    QFETCH(qreal, itemScale);
    QFETCH(float, outputScale);

    QVERIFY(resetPlainScene());
    QVERIFY2(applyViewportTransform(0, itemScale, WOutput::Normal, outputScale),
             "output dump failed after viewport scale");
    QVERIFY(expectTargetMoveIsPartial("viewport scale target"));
    QVERIFY(captureOutput(false).valid);
    QVERIFY(expectItemMoveIsPartial(named("blurPanel"), -35, 25, "viewport scale blur"));
}

void WSGDamageDebugVisualTest::highlight_viewportRotatedScaledItemMoves()
{
    QVERIFY(resetPlainScene());
    QVERIFY2(applyViewportTransform(90, 1.25, WOutput::R90, 1.f),
             "output dump failed after viewport rotate+scale");
    QVERIFY(expectTargetMoveIsPartial("rotated scaled target"));
    QVERIFY(captureOutput(false).valid);
    QVERIFY(expectItemMoveIsPartial(named("blurPanel"), -35, 25, "rotated scaled blur"));
}

void WSGDamageDebugVisualTest::client_commitDamageRegion()
{
    QVERIFY(resetPlainScene());
    const int clientFd = m_helper->createInProcessClientFd();
    QVERIFY2(clientFd >= 0, "failed to create in-process Wayland socketpair");

    DamageClient *client = damage_client_create(clientFd);
    QVERIFY2(client, "failed to create the C Wayland client");
    QFuture<void> clientFuture = QtConcurrent::run([client] {
        damage_client_run(client);
    });
    const auto stopClient = qScopeGuard([&] {
        damage_client_stop(client);
        clientFuture.waitForFinished();
        damage_client_destroy(client);
        waitUntilNoClientSurface();
    });

    auto pumpServer = [this] {
        m_helper->dispatchWaylandEvents();
        QTest::qWait(0);
    };

    QElapsedTimer timer;
    timer.start();
    while (!damage_client_is_mapped(client) && timer.elapsed() < 5000)
        pumpServer();
    if (!damage_client_is_mapped(client)) {
        const QByteArray err = damage_client_error(client);
        QFAIL(qPrintable(QStringLiteral("client did not map: %1").arg(QLatin1String(err))));
    }

    QQuickItem *surfaceItem = nullptr;
    while (!(surfaceItem = m_window->findChild<QQuickItem *>(QStringLiteral("clientSurface")))
           && timer.elapsed() < 5000) {
        pumpServer();
    }
    QVERIFY2(surfaceItem, "XdgToplevelSurfaceItem was not created for the client");

    auto *shellItem = qobject_cast<WSurfaceItem *>(surfaceItem);
    QVERIFY2(shellItem && shellItem->surface(), "client surface item has no WSurface");

    // First map reports the window AABB (DirtyNodeAdded), not buffer damage.
    for (int i = 0; i < 4; ++i) {
        pumpServer();
        QVERIFY2(captureOutput(false).valid, "output dump failed while idling the mapped client");
    }

    const QRect damage(8, 12, 16, 20);

    bool committed = false;
    const QMetaObject::Connection commitConn = connect(
        shellItem->surface(), &WSurface::commit, this,
        [&] { committed = true; }, Qt::DirectConnection);

    if (damage_client_commit_damage(client, damage.x(), damage.y(), damage.width(), damage.height()) != 0) {
        disconnect(commitConn);
        QFAIL("client failed to commit wl_surface.damage");
    }

    QElapsedTimer flushTimer;
    flushTimer.start();
    while (!committed && flushTimer.elapsed() < 2000)
        pumpServer();
    disconnect(commitConn);
    QVERIFY2(committed, "no scene-graph frame after the client damage commit");

    m_viewport->render(true);
    auto *br = WOutputViewportPrivate::get(m_viewport)->bufferRenderer;
    QVERIFY2(br, "no buffer renderer after the client damage commit");
    FrameSnap snap;
    snap.full = br->lastFlushIsFull();
    snap.region = br->lastFlushRegion();
    snap.valid = true;

    QQuickItem *content = shellItem->contentItem() ? shellItem->contentItem() : surfaceItem;
    const QRect mapped = content->mapRectToScene(QRectF(damage)).toAlignedRect();

    QVERIFY(expectRegionCovers(snap, mapped, "client damage"));
    QVERIFY(expectRegionAvoids(snap, sceneRect(named("sentinel")), "client damage sentinel"));
    QVERIFY(expectRegionAvoids(snap, sceneRect(named("target")), "client damage target"));
    QVERIFY(expectRegionNotFullscreen(snap, "client damage"));
}

void WSGDamageDebugVisualTest::rerender_hasNoOverlay()
{
    QVERIFY(resetPlainScene());
    auto *target = named("target");
    target->setX(target->x() + 70);
    const FrameSnap plain = captureOrSkip();

    QVERIFY(QMetaObject::invokeMethod(m_scene, "resetScene", Qt::DirectConnection));
    QVERIFY(captureOutput(false).valid);

    WSGDamageDebug::setMode(WSGDamageDebug::Mode::Rerender);
    target->setX(target->x() + 70);
    const FrameSnap rerendered = captureOrSkip(true);
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);

    QVERIFY(rerendered.full);
    QVERIFY2(regionDiffCount(plain.buffer, rerendered.buffer, outputRect(named("sentinel")), kRgbTolerance) == 0,
             "rerender changed the sentinel");
    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect (shadow) grabToImage is skipped");
    QVERIFY2(!rerendered.grab.isNull(), "rerender grabToImage failed");
    QVERIFY(expectImagesMatch(rerendered.buffer, rerendered.grab, "rerender painted a damage overlay"));
}

int runVisualTests(int argc, char *argv[])
{
    qputenv("WLR_BACKENDS", "headless");
    qunsetenv("WAYLAND_DISPLAY");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    WLog::init();
    WServer::initializeQPA();
    WRenderHelper::setupRendererBackend();

    QGuiApplication app(argc, argv);
    WSGDamageDebug::setMode(WSGDamageDebug::Mode::None);

    QQmlApplicationEngine engine;
#define WSG_STR(x) #x
#define WSG_XSTR(x) WSG_STR(x)
    engine.addImportPath(QLatin1String(WSG_XSTR(WAYLIB_QML_IMPORT_PATH)));
#undef WSG_XSTR
#undef WSG_STR

    QObject::connect(&engine, &QQmlEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &error : warnings)
            fprintf(stderr, "QML: %s\n", qPrintable(error.toString()));
    });

    engine.loadFromModule("WSGDamageVisual", "TestWindow");
    if (engine.rootObjects().isEmpty()) {
        fprintf(stderr, "QQmlApplicationEngine failed to load WSGDamageVisual/TestWindow\n");
        return 1;
    }

    auto *helper = engine.singletonInstance<VisualHelper *>("WSGDamageVisual", "Helper");
    if (!helper)
        return 1;

    auto *window = engine.rootObjects().first()->findChild<WOutputRenderWindow *>(
        QStringLiteral("renderWindow"));
    if (!window)
        return 1;
    helper->initProtocols(window, &engine);
    if (!helper->renderer()) {
        fprintf(stderr, "SKIP test_wsgdamage_visual: no wlroots renderer (no DRM device)\n");
        return 0;
    }
    window->setVisible(true);

    QSignalSpy initSpy(window, &WOutputRenderWindow::outputViewportInitialized);
    if (initSpy.isEmpty())
        initSpy.wait(5000);

    auto *viewport = window->findChild<WOutputViewport *>(QStringLiteral("outputViewport"));
    auto *scene = window->findChild<QQuickItem *>(QStringLiteral("scene"));
    if (!viewport || !scene)
        return 1;

    QTest::qWait(200);
    WSGDamageDebugVisualTest::setGlobals(window, viewport, scene, helper);

    WSGDamageDebugVisualTest test;
    const int result = QTest::qExec(&test, argc, argv);
    std::_Exit(result);
}

int main(int argc, char *argv[])
{
    if (!qEnvironmentVariableIsSet("WSG_DAMAGE_TEST_CHILD")
        && !qEnvironmentVariableIsSet("WLR_RENDERER")) {
        QCoreApplication app(argc, argv);
        int status = 0;
        const QStringList args = app.arguments().mid(1);
        const char *renderers[] = { "gles2", "vulkan", "pixman" };
        for (const char *renderer : renderers) {
            QProcess process;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("WLR_RENDERER"), QLatin1String(renderer));
            env.insert(QStringLiteral("WSG_DAMAGE_TEST_CHILD"), QStringLiteral("1"));
            process.setProcessEnvironment(env);
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            fprintf(stderr, "\n===== WLR_RENDERER=%s =====\n", renderer);
            process.start(app.applicationFilePath(), args);
            if (!process.waitForStarted(15000) || !process.waitForFinished(-1)) {
                fprintf(stderr, "WLR_RENDERER=%s failed to run: %s\n",
                        renderer, qPrintable(process.errorString()));
                return 1;
            }
            const int code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 1;
            fprintf(stderr, "===== WLR_RENDERER=%s exit %d =====\n", renderer, code);
            status |= code;
        }
        return status;
    }
    return runVisualTests(argc, argv);
}

#include "main.moc"

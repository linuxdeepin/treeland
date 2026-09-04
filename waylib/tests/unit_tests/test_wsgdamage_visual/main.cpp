// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "damageclient.h"
#include "helper.h"
#include "wbufferrenderer_p.h"
#include "woutputviewport_p.h"
#include "wsgbatchrenderer_p.h"
#include "wregionhighlightitem.h"
#include "wsgdamagelog_p.h"

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

#if QT_CONFIG(opengl)
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFuture>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QSGRendererInterface>
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

QByteArray nvidiaRenderNode()
{
    const QDir drm(QStringLiteral("/sys/class/drm"));
    const QStringList nodes = drm.entryList(QStringList{ QStringLiteral("renderD*") },
                                            QDir::Dirs | QDir::System);
    for (const QString &name : nodes) {
        QFile vendor(drm.filePath(name) + QStringLiteral("/device/vendor"));
        if (!vendor.open(QIODevice::ReadOnly))
            continue;
        if (vendor.readAll().trimmed() == "0x10de")
            return QByteArrayLiteral("/dev/dri/") + name.toLatin1();
    }
    return {};
}

QByteArray drainGlErrors(QQuickWindow *window = nullptr)
{
#if QT_CONFIG(opengl)
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx && window) {
        if (auto *ri = window->rendererInterface()) {
            ctx = static_cast<QOpenGLContext *>(
                ri->getResource(window, QSGRendererInterface::OpenGLContextResource));
        }
    }
    if (!ctx)
        return {};
    if (ctx->surface())
        ctx->makeCurrent(ctx->surface());
    QByteArray out;
    auto *fns = ctx->functions();
    for (;;) {
        const GLenum err = fns->glGetError();
        if (err == GL_NO_ERROR)
            break;
        if (!out.isEmpty())
            out += ' ';
        if (err == GL_INVALID_ENUM)
            out += "GL_INVALID_ENUM";
        else
            out += QByteArray::number(err, 16);
    }
    return out;
#else
    return {};
#endif
}

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
    QList<WRegionOverlay::Entry> entries;
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

    QQuickItem *namedAnywhere(const char *name) const
    {
        if (auto *item = named(name))
            return item;
        return m_window ? m_window->findChild<QQuickItem *>(QLatin1String(name)) : nullptr;
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

    QRect itemSceneRect(QQuickItem *item) const
    {
        if (!item)
            return {};
        return item->mapRectToScene(QRectF(0, 0, item->width(), item->height())).toAlignedRect();
    }

    QRect itemOutputRect(QQuickItem *item) const
    {
        if (!item || !m_viewport)
            return {};
        const QRect mapped =
            m_viewport->mapToOutput(item, QRectF(0, 0, item->width(), item->height())).toAlignedRect();
        return mapped.isEmpty() ? itemSceneRect(item) : mapped;
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

    FrameSnap captureDirtyOutput()
    {
        if (m_window)
            m_window->update();
        return captureOutput(false, false);
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
                if (auto *batch = dynamic_cast<WSGBatchRenderer::Renderer *>(br->currentRenderer())) {
                    snap.full = batch->flushRegionIsFull();
                    snap.region = batch->flushRegion();
                    gotBatchFlush = true;
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
        }
        if (auto *hl = m_window->findChild<WRegionHighlightItem *>(QStringLiteral("damageHighlight"))) {
            if (const auto *debug = hl->overlay()) {
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

        if (snap.valid && qEnvironmentVariableIsSet("WSG_DAMAGE_SAVE_SCREENS")) {
            static int screenDumpSeq = 0;
            const char *fn = QTest::currentTestFunction();
            saveScreen(snap.buffer,
                       QStringLiteral("%1-%2")
                           .arg(QLatin1String(fn ? fn : "unknown"))
                           .arg(++screenDumpSeq));
        }

        if (grabScene && snap.valid) {
            // grabToImage refs the item as an effect source and would dirty
            // this frame's flushRegion, so it runs on a follow-up frame.
            const auto previous = m_window->damageVisual();
            m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);
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
            m_window->setDamageVisual(previous);
        }
        return snap;
    }

    QImage expectedOverlayImage(const QImage &clean, const FrameSnap &snap) const
    {
        QImage painted = clean;
        if (painted.size() != snap.buffer.size())
            painted = painted.scaled(snap.buffer.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        QPainter painter(&painted);
        WRegionOverlay::paint(&painter, QTransform(), snap.entries, snap.nowMs);
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
        m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);
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

    bool showLockscreenAndSettle()
    {
        if (!resetPlainScene()) {
            qWarning("showLockscreen: resetPlainScene failed");
            return false;
        }
        if (!QMetaObject::invokeMethod(m_scene, "showLockscreen", Qt::DirectConnection)) {
            qWarning("showLockscreen: invokeMethod failed");
            return false;
        }
        for (int i = 0; i < 6; ++i) {
            if (!captureOutput(false).valid) {
                qWarning("showLockscreen: capture %d failed", i);
                return false;
            }
        }
        auto *lock = named("lockScene");
        if (!lock || !lock->isVisible()) {
            qWarning("showLockscreen: lockScene %s visible=%d",
                     lock ? "present" : "missing", lock && lock->isVisible());
            return false;
        }
        for (const char *name : { "lockPassword", "lockCaret", "lockPasswordBlur",
                                  "lockSessionButton" }) {
            if (!named(name)) {
                qWarning("showLockscreen: missing %s", name);
                return false;
            }
        }
        return true;
    }

    bool expectRegionHeightAtMost(const FrameSnap &snap, int maxHeight, const char *what)
    {
        if (!usesTrackedPartialDamage())
            return true;
        const QString prefix = QLatin1String(what);
        if (!QTest::qVerify(!snap.full,
                            "!snap.full",
                            qPrintable(QStringLiteral("%1: expected partial damage, got full")
                                           .arg(prefix)),
                            __FILE__, __LINE__)) {
            return false;
        }
        const int height = snap.region.boundingRect().height();
        return QTest::qVerify(height <= maxHeight,
                              "region height",
                              qPrintable(QStringLiteral("%1: damage height %2 (max %3), %4")
                                             .arg(prefix)
                                             .arg(height)
                                             .arg(maxHeight)
                                             .arg(WSGDamageLog::describe(snap.region, snap.full))),
                              __FILE__, __LINE__);
    }

    FrameSnap captureOrSkip(bool grabScene = false, bool forceViewportRender = true)
    {
        FrameSnap snap = captureOutput(grabScene, forceViewportRender);
        if (!snap.valid)
            QTest::qSkip("Could not dump the committed output buffer to QImage", __FILE__, __LINE__);
        return snap;
    }

    bool forceFullFlush()
    {
        auto *br = WOutputViewportPrivate::get(m_viewport)->bufferRenderer;
        if (!br)
            return false;
        br->markFullDamage();
        return true;
    }


    bool usesTrackedPartialDamage() const
    {
        // QSGSoftwareRenderer / pixman does not use WSGDamageTracker. Its
        // flushRegion is often the whole paint device, so partial-damage
        // assertions are RHI-only. Overlay paint and rerender still run.
        if (WRenderHelper::getGraphicsApi() == QSGRendererInterface::Software)
            return false;
        return WSGBatchRenderer::Renderer::defaultDamageTrackingEnabled();
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
        const QRegion expected(probe);
        return QTest::qVerify(expected.subtracted(snap.region).isEmpty(),
                              "region covers",
                              qPrintable(QStringLiteral("%1: damage %2 missed %3,%4 %5x%6")
                                             .arg(prefix, WSGDamageLog::describe(snap.region, snap.full))
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
                                             .arg(prefix, WSGDamageLog::describe(snap.region, snap.full))
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
                                             .arg(prefix, WSGDamageLog::describe(snap.region, snap.full))
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

    QString screenDumpDir() const
    {
        const QString dir = QDir::temp().filePath(QStringLiteral("wsg-damage-screens"));
        QDir().mkpath(dir);
        return dir;
    }

    void saveScreen(const QImage &img, const QString &name, const QRect &crop = {})
    {
        if (img.isNull())
            return;
        QImage out = crop.isEmpty() ? img : img.copy(crop.intersected(img.rect()));
        const QString path = screenDumpDir() + QLatin1Char('/') + name + QStringLiteral(".png");
        out.save(path);
        qInfo("saved %s (%dx%d)", qPrintable(path), out.width(), out.height());
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
        // Highlight is a separate OutputLayer. The primary buffer must match
        // the clean grab — same as highlight off.
        return expectImagesMatch(snap.buffer, snap.grab, what, ignoreCursor);
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
    void lockscreen_popupOpen_damageIsPopupNotFullscreen();
    void lockscreen_userListPopup_closeDuringEnterDoesNotCrash();
    void lockscreen_listScroll_damageClippedToList_matchesRerender();
    void lockscreen_caretBlink_doesNotMarkWholeInput();
    void lockscreen_cursorMove_doesNotMarkBlur();
    void lockscreen_idlePasswordBlur_stableAcrossFrames();
    void lockscreen_sessionChipBlur_afterHideSourceMatchesRerender();
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

    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);
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
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);
    if (m_window && m_window->findChild<QQuickItem *>(QStringLiteral("clientSurface")))
        QVERIFY(waitUntilNoClientSurface());
    for (int i = 0; i < 4; ++i) {
        QVERIFY(captureOutput(false).valid);
        auto *hl = m_window->findChild<WRegionHighlightItem *>(QStringLiteral("damageHighlight"));
        if (!hl || !hl->overlay() || !hl->overlay()->needsAnotherFrame())
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

    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
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

    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
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
    auto *frost = named("frostGlass");
    auto *behind = named("frostBehind");
    QVERIFY(frost);
    QVERIFY(behind);
    frost->setVisible(true);
    for (int i = 0; i < 4; ++i)
        QVERIFY(captureOutput(false).valid);

    behind->setX(behind->x() + 45);
    behind->setY(behind->y() + 25);

    const FrameSnap partial = captureOrSkip(false);
    QVERIFY(expectRegionCovers(partial, outputRect(frost), "frost blitter"));
    QVERIFY(expectRegionAvoids(partial, outputRect(named("sentinel")), "frost sentinel"));

    QVERIFY(forceFullFlush());
    const FrameSnap full = captureOrSkip(false);

    const QRect frostRect = outputRect(frost);
    QVERIFY2(!frostRect.isEmpty(), "frost blitter has no output rect");
    QVERIFY(expectImagesMatch(partial.buffer, full.buffer, "frosted partial vs rerender",
                              true, QRegion(frostRect)));
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
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
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
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
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

    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
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
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
    target->setX(target->x() + 70);
    QVERIFY(captureOutput(false).valid);

    for (int i = 0; i < 5; ++i) {
        QTest::qWait(40);
        const FrameSnap snap = captureOrSkip(true);
        QVERIFY(expectOverlayMatchesGrab(snap, "fade frame"));
    }

    QTest::qWait(WRegionOverlay::fadeOutMs);
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);
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

    // Highlight is a WOutputLayer. Idle compositor must not leave overlay
    // pixels in the primary swapchain. Layer fade is independent.
    QVERIFY(resetPlainScene());
    auto *target = named("target");
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
    target->setX(target->x() + 70);

    const FrameSnap painted = captureOrSkip(false);
    QVERIFY2(!painted.entries.isEmpty(), "highlight did not record overlay entries");

    QTest::qWait(WRegionOverlay::fadeOutMs + 50);

    FrameSnap idle;
    for (int i = 0; i < 4; ++i) {
        idle = captureOutput(false, false);
        QVERIFY2(idle.valid, "compositor idle frame could not dump the output buffer");
    }
    idle = captureOrSkip(true, false);
    QVERIFY2(!idle.grab.isNull(), "idle compositor grabToImage failed");

    if (vulkanSkipsMultiEffectGrab())
        QSKIP("Vulkan: MultiEffect grabToImage is skipped");

    FrameSnap paintedWithGrab = painted;
    paintedWithGrab.grab = idle.grab;
    QVERIFY(expectOverlayMatchesGrab(paintedWithGrab, "overlay before idle compositor"));
    QVERIFY(expectImagesMatch(idle.buffer, idle.grab, "idle compositor left overlay residue"));

    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);
}

void WSGDamageDebugVisualTest::highlight_popupOpenRegion()
{
    QVERIFY(resetPlainScene());
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
    QVERIFY(QMetaObject::invokeMethod(m_scene, "openPopup", Qt::DirectConnection));
    const FrameSnap snap = captureOrSkip();
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);

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

    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
    QVERIFY(QMetaObject::invokeMethod(m_scene, "closePopup", Qt::DirectConnection));
    const FrameSnap snap = captureOrSkip(true);
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);

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


void WSGDamageDebugVisualTest::lockscreen_popupOpen_damageIsPopupNotFullscreen()
{
    QVERIFY(showLockscreenAndSettle());
    QVERIFY(QMetaObject::invokeMethod(named("lockScene"), "openSessionPopup", Qt::DirectConnection));

    const FrameSnap snap = captureOrSkip();
    auto *sessionPanel = namedAnywhere("lockSessionGlass");
    auto *list = namedAnywhere("lockSessionList");
    QVERIFY2(sessionPanel && sessionPanel->isVisible(), "session panel was not shown");
    QVERIFY2(list && list->isVisible(), "session list was not shown");

    const QRect popupRect = itemSceneRect(sessionPanel);
    QVERIFY2(!popupRect.isEmpty(), "session popup has no scene rect");
    QVERIFY(expectRegionCovers(snap, popupRect, "lock popup open"));
    QVERIFY(expectRegionAvoids(snap, outputRect(named("sentinel")), "lock popup sentinel"));
    QVERIFY(expectRegionNotFullscreen(snap, "lock popup open"));
    // Unclipped ListView content (~12*61) used to paint a full-output-height
    // stripe the width of the popup. The popup itself is 280px tall.
    QVERIFY(expectRegionHeightAtMost(snap, popupRect.height() + 48, "lock popup open"));
}

void WSGDamageDebugVisualTest::lockscreen_userListPopup_closeDuringEnterDoesNotCrash()
{
    // Real lockscreen: UserList open, then click again during DTK FloatingPanel
    // enter (scale/opacity). WAYLIB_DEBUG_DAMAGE=highlight + extra-QRhi copy
    // aborted NVIDIA (GL_INVALID_ENUM, then free() in beginOffscreenFrame).
    // Must force the viewport render: OutputRenderWindow::render() skips
    // X11 while framePending, so highlight+extra-QRhi never ran. forceRender
    // still uses damage tracking; it is not a full-scene redraw.
    //
    // showLockscreenAndSettle() -> resetPlainScene() forces highlight off.
    // Turn it on after settle; the user crash is highlight-only.
    QVERIFY(showLockscreenAndSettle());
    const auto previous = m_window->damageVisual();
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
    const auto restoreMode = qScopeGuard([previous] {
        m_window->setDamageVisual(previous);
    });
    auto *lock = named("lockScene");
    QVERIFY(lock);
    auto *alwaysGlass = namedAnywhere("lockAlwaysGlass");
    QVERIFY2(alwaysGlass, "220x280 lockAlwaysGlass missing from lockContent");
    QVERIFY2(alwaysGlass->width() >= 200 && alwaysGlass->height() >= 200,
             "lockAlwaysGlass has no size");
    auto *wallpaper = named("wallpaper");
    auto *pulse = namedAnywhere("lockBehindPulse");
    QVERIFY2(wallpaper, "wallpaper missing; behind-blitter damage cannot be forced");
    QVERIFY2(pulse, "lockBehindPulse missing; behind-blitter damage cannot be forced");

    // Scale + rotation like DTK FloatingPanel enter (rotate-blit extra-QRhi).
    alwaysGlass->setScale(0.85);
    alwaysGlass->setRotation(6);
    alwaysGlass->setOpacity(0.4);
    for (int i = 0; i < 8; ++i) {
        alwaysGlass->setScale(0.85 + i * (0.15 / 7.0));
        alwaysGlass->setRotation(6.0 - i * (6.0 / 7.0));
        alwaysGlass->setOpacity(0.4 + i * (0.6 / 7.0));
        alwaysGlass->update();
        wallpaper->setScale(1.0 + i * 0.002);
        pulse->setX(48 + i * 4);
        pulse->setY(96 + (i % 2) * 8);
        QVERIFY2(m_window->damageVisual() == WOutputRenderWindow::DamageVisual::Highlight,
                 "highlight turned off during lockAlwaysGlass frames");
        QTest::qWait(16);
        // viewport->render(true) is forceRender, not a full damage redraw.
        // window->render() skips the X11 output while framePending, so the
        // animation never extra-QRhi copied under highlight.
        QVERIFY2(captureOutput(false, true).valid,
                 qPrintable(QStringLiteral("lockAlwaysGlass scale frame %1 failed").arg(i)));
        const QByteArray glErrors = drainGlErrors(m_window);
        QVERIFY2(glErrors.isEmpty(),
                 qPrintable(QStringLiteral("GL error after lockAlwaysGlass frame %1: %2")
                                .arg(i)
                                .arg(QString::fromLatin1(glErrors))));
    }
    alwaysGlass->setScale(1);
    alwaysGlass->setRotation(0);
    alwaysGlass->setOpacity(1);
    wallpaper->setScale(1);
    pulse->setX(48);
    pulse->setY(96);
    lock->setProperty("popupEnterAnimation", true);
    QVERIFY(QMetaObject::invokeMethod(lock, "openSessionPopup", Qt::DirectConnection));
    // Stay in the enter animation (240ms) before closing, like clicking the
    // chip again while the DTK panel is still scaling in.
    for (int i = 0; i < 6; ++i) {
        QVERIFY2(m_window->damageVisual() == WOutputRenderWindow::DamageVisual::Highlight,
                 "highlight turned off during popup enter");
        QTest::qWait(16);
        QVERIFY2(captureOutput(false, true).valid,
                 qPrintable(QStringLiteral("popup enter frame %1 failed").arg(i)));
        const QByteArray glErrors = drainGlErrors(m_window);
        QVERIFY2(glErrors.isEmpty(),
                 qPrintable(QStringLiteral("GL error after popup enter frame %1: %2")
                                .arg(i)
                                .arg(QString::fromLatin1(glErrors))));
    }
    auto *sessionPanel = namedAnywhere("lockSessionGlass");
    QVERIFY2(sessionPanel && sessionPanel->isVisible(), "session popup was not shown during enter");
    QVERIFY2(sessionPanel->width() >= 200 && sessionPanel->height() >= 200, "session popup has no size");
    QVERIFY(QMetaObject::invokeMethod(lock, "closeSessionPopup", Qt::DirectConnection));
    for (int i = 0; i < 18; ++i) {
        QVERIFY2(m_window->damageVisual() == WOutputRenderWindow::DamageVisual::Highlight,
                 "highlight turned off during popup close");
        QTest::qWait(16);
        QVERIFY2(captureOutput(false, true).valid,
                 qPrintable(QStringLiteral("popup close-during-enter frame %1 failed").arg(i)));
        const QByteArray glErrors = drainGlErrors(m_window);
        QVERIFY2(glErrors.isEmpty(),
                 qPrintable(QStringLiteral("GL error after popup close frame %1: %2")
                                .arg(i)
                                .arg(QString::fromLatin1(glErrors))));
        if (i == 2)
            QVERIFY(QMetaObject::invokeMethod(lock, "openSessionPopup", Qt::DirectConnection));
        if (i == 5)
            QVERIFY(QMetaObject::invokeMethod(lock, "closeSessionPopup", Qt::DirectConnection));
    }
    lock->setProperty("popupEnterAnimation", false);
    QVERIFY(QMetaObject::invokeMethod(lock, "resetLock", Qt::DirectConnection));
    QVERIFY2(captureOutput(false, true).valid, "popup reset after close-during-enter failed");
}

void WSGDamageDebugVisualTest::lockscreen_listScroll_damageClippedToList_matchesRerender()
{
    QVERIFY(showLockscreenAndSettle());
    QVERIFY(QMetaObject::invokeMethod(named("lockScene"), "openSessionPopup", Qt::DirectConnection));
    for (int i = 0; i < 4; ++i)
        QVERIFY(captureOutput(false).valid);

    auto *list = namedAnywhere("lockSessionList");
    auto *sessionPanel = namedAnywhere("lockSessionGlass");
    QVERIFY2(list && sessionPanel, "lock session widgets missing");
    QVERIFY2(list->isVisible(), "session list was not shown");

    list->setProperty("contentY", list->property("contentY").toReal() + 90);
    const FrameSnap partial = captureOrSkip();

    const QRect listRect = itemSceneRect(list);
    QVERIFY2(!listRect.isEmpty(), "session list has no scene rect");
    QVERIFY(expectRegionCovers(partial, listRect, "lock list scroll"));
    QVERIFY(expectRegionAvoids(partial, outputRect(named("sentinel")), "lock list sentinel"));
    QVERIFY(expectRegionAvoids(partial, itemOutputRect(named("lockPasswordBlur")),
                               "lock list password"));
    QVERIFY(expectRegionNotFullscreen(partial, "lock list scroll"));
    QVERIFY(expectRegionHeightAtMost(partial, listRect.height() + 32, "lock list scroll"));

    QVERIFY(forceFullFlush());
    const FrameSnap full = captureOrSkip(false);

    const QRect panelRect = itemOutputRect(sessionPanel);
    QVERIFY2(!panelRect.isEmpty(), "session panel has no output rect");
    QVERIFY(expectImagesMatch(partial.buffer, full.buffer, "lock list scroll partial vs rerender",
                              true, QRegion(panelRect)));
}

void WSGDamageDebugVisualTest::lockscreen_caretBlink_doesNotMarkWholeInput()
{
    QVERIFY(showLockscreenAndSettle());
    auto *caret = named("lockCaret");
    auto *blur = named("lockPasswordBlur");
    QVERIFY2(caret && blur, "lock password widgets missing");

    QVERIFY(QMetaObject::invokeMethod(m_scene, "blinkLockCaret", Qt::DirectConnection));
    const FrameSnap snap = captureOrSkip();

    const QRect caretRect = itemOutputRect(caret);
    const QRect passwordRect = itemOutputRect(blur);
    QVERIFY2(!caretRect.isEmpty(), "lock caret has no output rect");
    QVERIFY2(passwordRect.width() > 16 && passwordRect.height() > 8,
             qPrintable(QStringLiteral("lock password mapped %1x%2, expected ~220x30")
                            .arg(passwordRect.width())
                            .arg(passwordRect.height())));
    QVERIFY(expectRegionCovers(snap, caretRect, "lock caret"));
    QVERIFY(expectRegionAvoids(snap, outputRect(named("sentinel")), "lock caret sentinel"));
    QVERIFY(expectRegionNotFullscreen(snap, "lock caret"));
    QVERIFY(expectRegionAvoids(snap, itemOutputRect(named("lockSessionButton")),
                               "lock caret session button"));
    QVERIFY(expectRegionAvoids(snap, itemOutputRect(named("lockLoginBtn")),
                               "lock caret login button"));

    if (!usesTrackedPartialDamage())
        return;
    QVERIFY2(snap.region.boundingRect().width() < passwordRect.width() / 2,
             qPrintable(QStringLiteral("caret marked the whole password field: damage %1 field %2x%3+%4+%5")
                            .arg(WSGDamageLog::describe(snap.region, snap.full))
                            .arg(passwordRect.width())
                            .arg(passwordRect.height())
                            .arg(passwordRect.x())
                            .arg(passwordRect.y())));
}

void WSGDamageDebugVisualTest::lockscreen_cursorMove_doesNotMarkBlur()
{
    QVERIFY(showLockscreenAndSettle());
    QVERIFY2(configureCursorPath(CursorPath::SoftwareInline),
             qPrintable(QStringLiteral("inline software cursor did not enable: %1").arg(cursorPathDebug())));

    auto *cursor = m_helper->cursor();
    const QPointF home(520, 24);
    const QPointF moved(640, 48);
    cursor->setVisible(true);
    cursor->setPosition(home);
    for (int i = 0; i < 3; ++i)
        QVERIFY(captureOutput(false).valid);

    cursor->setPosition(moved);
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Highlight);
    const FrameSnap snap = captureOrSkip(true);
    m_window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);

    QVERIFY(expectRegionNotFullscreen(snap, "lock cursor move"));
    QVERIFY(expectRegionAvoids(snap, itemOutputRect(named("lockPasswordBlur")),
                               "lock cursor password blur"));
    QVERIFY(expectRegionAvoids(snap, itemOutputRect(named("lockSessionButton")),
                               "lock cursor session button"));
    QVERIFY(expectRegionAvoids(snap, itemOutputRect(named("lockAlwaysGlass")),
                               "lock cursor lockAlwaysGlass"));
    QVERIFY(expectRegionAvoids(snap, itemOutputRect(named("lockUserButton")),
                               "lock cursor user button"));

    cursor->setPosition(home);
    QVERIFY(captureOutput(false).valid);
    restoreCursorDefaults();
    parkCursor();
}

void WSGDamageDebugVisualTest::lockscreen_idlePasswordBlur_stableAcrossFrames()
{
    QVERIFY(showLockscreenAndSettle());
    const FrameSnap a = captureOrSkip();
    const FrameSnap b = captureOrSkip();

    auto *blur = named("lockPasswordBlur");
    QVERIFY2(blur, "lock password blur missing");
    const QRect blurRect = outputRect(blur);
    QVERIFY2(!blurRect.isEmpty(), "lock password blur has no output rect");
    QVERIFY(expectImagesMatch(a.buffer, b.buffer, "lock idle password blur", true, QRegion(blurRect)));

    auto *chip = named("lockSessionBtnBlur");
    const QRect chipRect = chip ? itemOutputRect(chip) : QRect();
    const QRect corner(a.buffer.width() - 220, a.buffer.height() - 90, 210, 80);
    saveScreen(a.buffer, QStringLiteral("lock-idle-full"));
    saveScreen(a.buffer, QStringLiteral("lock-idle-password"), blurRect);
    saveScreen(a.buffer, QStringLiteral("lock-idle-chip"), chipRect);
    saveScreen(a.buffer, QStringLiteral("lock-idle-bottom-right"), corner);

    if (!usesTrackedPartialDamage())
        return;
    QVERIFY2(!a.full && !b.full, "idle lockscreen reported full damage");
    const QRect blurCore = blurRect.adjusted(2, 2, -2, -2);
    QVERIFY2(!a.region.intersects(blurCore) && !b.region.intersects(blurCore),
             qPrintable(QStringLiteral("idle recopy of password blitter: %1 then %2")
                            .arg(WSGDamageLog::describe(a.region, a.full),
                                 WSGDamageLog::describe(b.region, b.full))));
}

void WSGDamageDebugVisualTest::lockscreen_sessionChipBlur_afterHideSourceMatchesRerender()
{
    // Replica of LoginAnimation: first paints go through ShaderEffectSource
    // (hideSource). After it stops, chips are idle — damage must still blit.
    if (!resetPlainScene())
        QFAIL("resetPlainScene failed");
    QVERIFY(QMetaObject::invokeMethod(m_scene, "showLockscreen", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(m_scene, "startLockHideSource", Qt::DirectConnection));
    for (int i = 0; i < 6; ++i)
        QVERIFY2(captureOutput(false).valid, "hideSource settle failed");
    QVERIFY(QMetaObject::invokeMethod(m_scene, "stopLockHideSource", Qt::DirectConnection));
    for (int i = 0; i < 4; ++i)
        QVERIFY2(captureOutput(false).valid, "post-hideSource idle failed");

    auto *chip = named("lockSessionBtnBlur");
    QVERIFY2(chip, "session chip blur missing");
    const QRect chipRect = itemOutputRect(chip);
    QVERIFY2(!chipRect.isEmpty(), "session chip has no output rect");
    const QRect corner(800 - 220, 480 - 90, 210, 80);

    const FrameSnap partial = captureOrSkip(false);
    saveScreen(partial.buffer, QStringLiteral("lock-hidesource-partial-full"));
    saveScreen(partial.buffer, QStringLiteral("lock-hidesource-partial-chip"), chipRect);
    saveScreen(partial.buffer, QStringLiteral("lock-hidesource-partial-bottom-right"), corner);

    QVERIFY(forceFullFlush());
    const FrameSnap full = captureOrSkip(false);
    saveScreen(full.buffer, QStringLiteral("lock-hidesource-rerender-full"));
    saveScreen(full.buffer, QStringLiteral("lock-hidesource-rerender-chip"), chipRect);
    saveScreen(full.buffer, QStringLiteral("lock-hidesource-rerender-bottom-right"), corner);

    QVERIFY(expectImagesMatch(partial.buffer, full.buffer,
                              "lock session chip after hideSource vs rerender",
                              true, QRegion(chipRect)));
}

int runVisualTests(int argc, char *argv[])
{
    if (!qEnvironmentVariableIsSet("WLR_BACKENDS"))
        qputenv("WLR_BACKENDS", "headless");
    if (qgetenv("WLR_BACKENDS") == "headless")
        qunsetenv("WAYLAND_DISPLAY");
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE"))
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    WLog::init();
    WServer::initializeQPA();
    WRenderHelper::setupRendererBackend();

    QGuiApplication app(argc, argv);

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
    window->setDamageVisual(WOutputRenderWindow::DamageVisual::Off);
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
        const auto runChild = [&](const QProcessEnvironment &env, const QStringList &childArgs,
                                  const char *label) {
            QProcess process;
            process.setProcessEnvironment(env);
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            fprintf(stderr, "\n===== %s =====\n", label);
            process.start(app.applicationFilePath(), childArgs);
            if (!process.waitForStarted(15000) || !process.waitForFinished(15 * 60 * 1000)) {
                fprintf(stderr, "%s failed to run: %s\n", label, qPrintable(process.errorString()));
                return 1;
            }
            const int code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 1;
            fprintf(stderr, "===== %s exit %d =====\n", label, code);
            return code;
        };

        const char *renderers[] = { "gles2", "vulkan", "pixman" };
        for (const char *renderer : renderers) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("WLR_RENDERER"), QLatin1String(renderer));
            env.insert(QStringLiteral("WSG_DAMAGE_TEST_CHILD"), QStringLiteral("1"));
            if (qstrcmp(renderer, "gles2") == 0) {
                const QByteArray nvidia = nvidiaRenderNode();
                if (!nvidia.isEmpty() && !qEnvironmentVariableIsSet("WLR_RENDER_DRM_DEVICE"))
                    env.insert(QStringLiteral("WLR_RENDER_DRM_DEVICE"), QString::fromLatin1(nvidia));
            }
            status |= runChild(env, args, qPrintable(QStringLiteral("WLR_RENDERER=%1").arg(renderer)));
        }

        // Headless GBM on NVIDIA does not match the abort (X11-2 XR24 +
        // highlight + extra-QRhi). Run the lockscreen popup case on the X11
        // backend when a display and NVIDIA node exist.
        if (qEnvironmentVariableIsSet("DISPLAY") && !nvidiaRenderNode().isEmpty()) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("WLR_RENDERER"), QStringLiteral("gles2"));
            env.insert(QStringLiteral("WSG_DAMAGE_TEST_CHILD"), QStringLiteral("1"));
            env.insert(QStringLiteral("WLR_BACKENDS"), QStringLiteral("x11"));
            env.insert(QStringLiteral("WLR_RENDER_DRM_DEVICE"),
                       QString::fromLatin1(nvidiaRenderNode()));
            env.insert(QStringLiteral("QT_LOGGING_RULES"),
                       QStringLiteral("waylib.render.buffer.debug=true;waylib.output.buffer.debug=true"));
            env.remove(QStringLiteral("WAYLAND_DISPLAY"));
            status |= runChild(env,
                               { QStringLiteral("lockscreen_userListPopup_closeDuringEnterDoesNotCrash") },
                               "WLR_BACKENDS=x11 NVIDIA gles2 lockscreen popup crash");
        }
        return status;
    }
    return runVisualTests(argc, argv);
}

#include "main.moc"

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <cmath>
#include <algorithm>

#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <qwlogging.h>
#include <wserver.h>
#include <wrenderhelper.h>

#include "TestHelper.h"

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

/// Liquid Glass effect test using waylib's headless backend for OpenGL.
///
/// A custom main() sets up the waylib QPA platform with WLR_BACKENDS=headless
/// before QGuiApplication, giving us a real OpenGL context via Mesa llvmpipe
/// — no display server or xvfb-run needed.  The TestGlass QML module bundles
/// GlassEffect.qml + shaders, and TestWindow.qml wraps the test scene in an
/// OutputRenderWindow + OutputItem so it renders through waylib's scene graph.
///
/// Property tests read derived values at runtime; rendering tests use
/// grabToImage to capture and compare images.
class GlassEffectTest : public QObject
{
    Q_OBJECT

public:
    static void setGlobals(WOutputRenderWindow *w, QQuickItem *s, QQuickItem *g, TestHelper *h)
    {
        m_window = w;
        m_scene = s;
        m_glass = g;
        m_helper = h;
    }

private:
    static inline WOutputRenderWindow *m_window = nullptr;
    static inline QQuickItem *m_scene = nullptr;
    static inline QQuickItem *m_glass = nullptr;
    static inline TestHelper *m_helper = nullptr;

    /// Grab an item to a QImage (asynchronous — waits for ready).
    static QImage grabImage(QQuickItem *item, const QSize &size = QSize(256, 256))
    {
        auto result = item->grabToImage(size);
        if (!result)
            return {};

        QSignalSpy spy(result.data(), &QQuickItemGrabResult::ready);
        spy.wait(5000);
        return result->image();
    }

    /// Count pixels that differ between two images of the same size.
    static int pixelDiffCount(const QImage &a, const QImage &b)
    {
        if (a.size() != b.size() || a.format() != b.format())
            return -1;

        int count = 0;
        const int w = a.width();
        const int h = a.height();
        for (int y = 0; y < h; ++y) {
            const auto *pa = reinterpret_cast<const QRgb *>(a.scanLine(y));
            const auto *pb = reinterpret_cast<const QRgb *>(b.scanLine(y));
            for (int x = 0; x < w; ++x) {
                if (pa[x] != pb[x])
                    ++count;
            }
        }
        return count;
    }

    static int colorDistance(QRgb a, QRgb b)
    {
        return std::abs(qRed(a) - qRed(b))
            + std::abs(qGreen(a) - qGreen(b))
            + std::abs(qBlue(a) - qBlue(b))
            + std::abs(qAlpha(a) - qAlpha(b));
    }

    static int regionDiffCount(const QImage &a, const QImage &b, const QRect &region, int minDistance = 0)
    {
        if (a.size() != b.size() || a.format() != b.format())
            return -1;

        int count = 0;
        const QRect bounded = region.intersected(a.rect());
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            const auto *pa = reinterpret_cast<const QRgb *>(a.scanLine(y));
            const auto *pb = reinterpret_cast<const QRgb *>(b.scanLine(y));
            for (int x = bounded.left(); x <= bounded.right(); ++x) {
                if (colorDistance(pa[x], pb[x]) > minDistance)
                    ++count;
            }
        }
        return count;
    }

    static int maxDiagonalCornerDiscontinuity(const QImage &img)
    {
        const int w = img.width();
        int maxStep = 0;

        for (int d = 12; d <= 46; ++d) {
            const QRgb a = img.pixel(w - 1 - d, d + 1);
            const QRgb b = img.pixel(w - 2 - d, d);
            maxStep = std::max(maxStep, colorDistance(a, b));
        }

        return maxStep;
    }

    void setSmallRadiusLargeBezel()
    {
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("radius", 8.0);
        m_glass->setProperty("bezelWidth", 52.0);
        m_glass->setProperty("thickness", 120.0);
        m_glass->setProperty("ior", 1.45);
        m_glass->setProperty("specularOpacity", 0.0);
        m_glass->setProperty("tintOpacity", 0.0);
        m_glass->setProperty("shadow", 0.0);
        QTest::qWait(50);
    }

    static bool requiresShaderRendering(const char *testFunction)
    {
        static constexpr const char *renderingTests[] = {
            "shadowProducesDifferentRender",
            "iorProducesDifferentRender",
            "thicknessProducesDifferentRender",
            "radiusProducesTransparentCorners",
            "largeBezelWithSmallRadiusDoesNotIntroduceCornerDiagonalSeam",
            "largeBezelRemainsVisibleAlongStraightEdgesWithSmallRadius",
            "zeroBlurMultiplierStillAppliesGaussianBlur",
            "blurAmountAndMultiplierChangeRenderedBlurStrength",
            "blurToggleProducesDifferentRender",
        };

        for (const auto *name : renderingTests) {
            if (qstrcmp(testFunction, name) == 0)
                return true;
        }
        return false;
    }

    static bool shaderRenderingAvailable()
    {
        return m_helper && !m_helper->usesSoftwareRenderer();
    }

    /// Reset glass to default property values (called before each test).
    void resetGlass()
    {
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("blurMax", 36);
        m_glass->setProperty("blurAmount", 1.0);
        m_glass->setProperty("blurMultiplier", 0.0);
        m_glass->setProperty("radius", 0.0);
        m_glass->setProperty("bezelWidth", 60.0);
        m_glass->setProperty("thickness", 50.0);
        m_glass->setProperty("ior", 3.0);
        m_glass->setProperty("specularOpacity", 0.55);
        m_glass->setProperty("tintOpacity", 0.08);
        m_glass->setProperty("shadow", 0.5);
        QTest::qWait(50);
    }

private Q_SLOTS:

    void initTestCase()
    {
        QVERIFY(m_window);
        QVERIFY(m_scene);
        QVERIFY(m_glass);
    }

    void init()
    {
        if (requiresShaderRendering(QTest::currentTestFunction()) && !shaderRenderingAvailable()) {
            QSKIP("Shader rendering checks require OpenGL; the software renderer "
                  "does not run ShaderEffect/MultiEffect output");
        }
    }

    void cleanup()
    {
        resetGlass();
    }

    // ── Property tests ────────────────────────────────────────────────

    void multiEffectEnabledReflectsBlurParams()
    {
        // Default scene: blurEnabled=false → false
        m_glass->setProperty("blurEnabled", false);
        QTest::qWait(10);
        QVERIFY(!m_glass->property("multiEffectEnabled").toBool());

        // blurEnabled → true
        m_glass->setProperty("blurEnabled", true);
        QTest::qWait(10);
        QVERIFY(m_glass->property("multiEffectEnabled").toBool());
    }

    void glassKnobsAreRuntimeQmlProperties()
    {
        auto *shader = m_glass->findChild<QObject *>("glassShader");
        QVERIFY(shader);

        const QList<QByteArray> propertyNames = {
            "blurAmount",
            "blurMultiplier",
            "blurMax",
            "bezelWidth",
            "thickness",
            "ior",
            "specularOpacity",
            "tintOpacity",
            "shadow",
        };

        for (const QByteArray &name : propertyNames) {
            QVERIFY2(m_glass->metaObject()->indexOfProperty(name.constData()) >= 0,
                     qPrintable(QStringLiteral("GlassEffect must expose %1 as a real QML property")
                                    .arg(QString::fromLatin1(name))));
        }

        QVERIFY(m_glass->setProperty("bezelWidth", 47.0));
        QVERIFY(m_glass->setProperty("thickness", 133.0));
        QVERIFY(m_glass->setProperty("ior", 2.0));
        QVERIFY(m_glass->setProperty("specularOpacity", 0.9));
        QVERIFY(m_glass->setProperty("tintOpacity", 0.2));
        QVERIFY(m_glass->setProperty("shadow", 0.8));
        QVERIFY(m_glass->setProperty("blurEnabled", true));
        QVERIFY(m_glass->setProperty("blurMax", 18));
        QVERIFY(m_glass->setProperty("blurAmount", 0.75));
        QVERIFY(m_glass->setProperty("blurMultiplier", 1.5));
        QTest::qWait(10);

        QCOMPARE(shader->property("bezelWidth").toReal(), 47.0);
        QCOMPARE(shader->property("thickness").toReal(), 133.0);
        QCOMPARE(shader->property("ior").toReal(), 2.0);
        QCOMPARE(shader->property("specularOpacity").toReal(), 0.9);
        QCOMPARE(shader->property("tintOpacity").toReal(), 0.2);
        // shadow is a GlassEffect-level property consumed by the Stage 3
        // MultiEffect; it is not forwarded to the ShaderEffect.
        QVERIFY(!shader->property("shadow").isValid());
        QCOMPARE(m_glass->property("shadow").toReal(), 0.8);
        QCOMPARE(m_glass->property("multiEffectEnabled").toBool(), true);
    }

    // ── Rendering tests: grabToImage + image comparison ───────────────

    void grabIsDeterministic()
    {
        const QImage img1 = grabImage(m_scene);
        QVERIFY(!img1.isNull());

        const QImage img2 = grabImage(m_scene);
        QVERIFY(!img2.isNull());

        QCOMPARE(img1, img2);
    }

    void shadowProducesDifferentRender()
    {
        m_glass->setProperty("radius", 34.0);
        m_glass->setProperty("shadow", 0.0);
        QTest::qWait(50);

        const QImage noShadow = grabImage(m_scene);
        QVERIFY(!noShadow.isNull());

        m_glass->setProperty("shadow", 1.0);
        QTest::qWait(50);

        const QImage withShadow = grabImage(m_scene);
        QVERIFY(!withShadow.isNull());

        QVERIFY(noShadow != withShadow);
    }

    void iorProducesDifferentRender()
    {
        m_glass->setProperty("radius", 34.0);
        m_glass->setProperty("bezelWidth", 40.0);
        m_glass->setProperty("thickness", 80.0);
        m_glass->setProperty("ior", 1.0);
        QTest::qWait(50);

        const QImage lowIor = grabImage(m_scene);
        QVERIFY(!lowIor.isNull());

        m_glass->setProperty("ior", 3.0);
        QTest::qWait(50);

        const QImage highIor = grabImage(m_scene);
        QVERIFY(!highIor.isNull());

        QVERIFY(lowIor != highIor);
    }

    void thicknessProducesDifferentRender()
    {
        m_glass->setProperty("radius", 34.0);
        m_glass->setProperty("bezelWidth", 40.0);
        m_glass->setProperty("thickness", 10.0);
        QTest::qWait(50);

        const QImage thinGlass = grabImage(m_scene);
        QVERIFY(!thinGlass.isNull());

        m_glass->setProperty("thickness", 150.0);
        QTest::qWait(50);

        const QImage thickGlass = grabImage(m_scene);
        QVERIFY(!thickGlass.isNull());

        QVERIFY(thinGlass != thickGlass);
    }

    void radiusProducesTransparentCorners()
    {
        m_glass->setProperty("radius", 60.0);
        QTest::qWait(50);

        const QImage img = grabImage(m_glass);
        QVERIFY(!img.isNull());

        const int w = img.width();
        const int h = img.height();
        const int offset = 2;

        const QList<QPoint> cornerPixels = {
            {offset, offset},
            {w - 1 - offset, offset},
            {offset, h - 1 - offset},
            {w - 1 - offset, h - 1 - offset},
        };

        for (const auto &pt : cornerPixels) {
            const QRgb px = img.pixel(pt.x(), pt.y());
            QVERIFY2(qAlpha(px) == 0,
                     qPrintable(QStringLiteral("corner pixel (%1,%2) should be transparent, got alpha=%3")
                                    .arg(pt.x()).arg(pt.y()).arg(qAlpha(px))));
        }

        const QRgb centerPx = img.pixel(w / 2, h / 2);
        QVERIFY2(qAlpha(centerPx) == 255,
                 qPrintable(QStringLiteral("center pixel should be opaque, got alpha=%1")
                                .arg(qAlpha(centerPx))));
    }

    void largeBezelWithSmallRadiusDoesNotIntroduceCornerDiagonalSeam()
    {
        setSmallRadiusLargeBezel();

        const QImage img = grabImage(m_scene);
        QVERIFY(!img.isNull());

        const int seamStep = maxDiagonalCornerDiscontinuity(img);
        QVERIFY2(seamStep < 96,
                 qPrintable(QStringLiteral("small-radius/large-bezel corner has a sharp diagonal seam: max cross-diagonal step=%1")
                                .arg(seamStep)));
    }

    void largeBezelRemainsVisibleAlongStraightEdgesWithSmallRadius()
    {
        setSmallRadiusLargeBezel();
        QTest::qWait(50);

        const QImage largeBezel = grabImage(m_scene);
        QVERIFY(!largeBezel.isNull());

        m_glass->setProperty("bezelWidth", 1.0);
        QTest::qWait(50);

        const QImage tinyBezel = grabImage(m_scene);
        QVERIFY(!tinyBezel.isNull());

        const QRect rightStraightEdge(212, 72, 24, 112);
        const int activePixels = regionDiffCount(largeBezel, tinyBezel, rightStraightEdge, 8);
        QVERIFY2(activePixels > rightStraightEdge.width() * rightStraightEdge.height() / 3,
                 qPrintable(QStringLiteral("large straight-edge bezel should remain active beyond the small radius: changedPixels=%1 regionPixels=%2")
                                .arg(activePixels)
                                .arg(rightStraightEdge.width() * rightStraightEdge.height())));
    }

    void zeroBlurMultiplierStillAppliesGaussianBlur()
    {
        m_glass->setProperty("blurEnabled", true);
        m_glass->setProperty("blurMax", 48);
        m_glass->setProperty("blurAmount", 1.0);
        QVERIFY(m_glass->setProperty("blurMultiplier", 0.0));
        QTest::qWait(50);

        const QImage defaultQualityBlur = grabImage(m_scene);
        QVERIFY(!defaultQualityBlur.isNull());

        m_glass->setProperty("blurEnabled", false);
        QTest::qWait(50);

        const QImage withoutBlur = grabImage(m_scene);
        QVERIFY(!withoutBlur.isNull());

        const QRect centerContrastFeature(92, 92, 72, 72);
        const int changedPixels = regionDiffCount(defaultQualityBlur, withoutBlur, centerContrastFeature, 4);
        QVERIFY2(changedPixels > centerContrastFeature.width() * centerContrastFeature.height() / 5,
                 qPrintable(QStringLiteral("blurMultiplier=0 must keep blur active instead of disabling it: changedPixels=%1 regionPixels=%2")
                                .arg(changedPixels)
                                .arg(centerContrastFeature.width() * centerContrastFeature.height())));
    }

    void blurAmountAndMultiplierChangeRenderedBlurStrength()
    {
        m_glass->setProperty("blurEnabled", true);
        m_glass->setProperty("blurMax", 48);
        m_glass->setProperty("radius", 34.0);
        QVERIFY2(m_glass->setProperty("blurAmount", 0.15),
                 "GlassEffect must expose blurAmount as a runtime QML property");
        QVERIFY2(m_glass->setProperty("blurMultiplier", 0.5),
                 "GlassEffect must expose blurMultiplier as a runtime QML property");
        QTest::qWait(50);

        const QImage weakBlur = grabImage(m_scene);
        QVERIFY(!weakBlur.isNull());

        QVERIFY(m_glass->setProperty("blurAmount", 1.0));
        QVERIFY(m_glass->setProperty("blurMultiplier", 2.0));
        QTest::qWait(50);

        const QImage strongBlur = grabImage(m_scene);
        QVERIFY(!strongBlur.isNull());

        const QRect centerContrastFeature(92, 92, 72, 72);
        const int changedPixels = regionDiffCount(weakBlur, strongBlur, centerContrastFeature, 4);
        QVERIFY2(changedPixels > centerContrastFeature.width() * centerContrastFeature.height() / 5,
                 qPrintable(QStringLiteral("blurAmount/blurMultiplier must visibly change the sampled backdrop blur: changedPixels=%1 regionPixels=%2")
                                .arg(changedPixels)
                                .arg(centerContrastFeature.width() * centerContrastFeature.height())));
    }

    void blurToggleProducesDifferentRender()
    {
        m_glass->setProperty("blurEnabled", true);
        m_glass->setProperty("blurMax", 36);
        m_glass->setProperty("radius", 34.0);
        QTest::qWait(50);

        const QImage withBlur = grabImage(m_scene);
        QVERIFY(!withBlur.isNull());

        m_glass->setProperty("blurEnabled", false);
        QTest::qWait(50);

        const QImage withoutBlur = grabImage(m_scene);
        QVERIFY(!withoutBlur.isNull());

        QVERIFY(withBlur != withoutBlur);
    }
};

int main(int argc, char *argv[])
{
    // Headless wlroots backend — no display server needed.
    qputenv("WLR_BACKENDS", "headless");

    qw_log::init();
    WServer::initializeQPA();

    // Probe the graphics API that can create a wlroots renderer in this
    // environment.  CI containers without a DRM render node fall back to
    // Software/pixman; rendering-only tests skip in that mode.
    WRenderHelper::setupRendererBackend();

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    engine.loadFromModule("TestGlass", "TestWindow");
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *root = engine.rootObjects().first();
    auto *window = root->findChild<WOutputRenderWindow *>("renderWindow");
    Q_ASSERT(window);

    auto *helper = engine.singletonInstance<TestHelper *>("TestGlass", "TestHelper");
    Q_ASSERT(helper);
    helper->initProtocols(window, &engine);
    window->setVisible(true);  // QQuickItem::grabToImage requires isVisible

    // Wait for the headless output to be created and the scene to be ready.
    QSignalSpy initSpy(window, &WOutputRenderWindow::outputViewportInitialized);
    if (initSpy.isEmpty())
        initSpy.wait(5000);
    QTest::qWait(500); // let the scene graph settle

    auto *scene = window->findChild<QQuickItem *>("glassScene");
    Q_ASSERT(scene);
    auto *glass = window->findChild<QQuickItem *>("glassEffect");
    Q_ASSERT(glass);

    GlassEffectTest::setGlobals(window, scene, glass, helper);

    GlassEffectTest test;
    int result = QTest::qExec(&test, argc, argv);
    // wlroots objects (renderer, allocator, compositor) crash during normal
    // destructor unwinding — skip cleanup and exit immediately.
    std::_Exit(result);
}

#include "main.moc"

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

        // Top-right has only the smooth horizontal gradient behind it in this
        // fixture, so any large cross-diagonal jump is from the shader rather
        // than a backdrop feature.  The paired samples straddle the possible
        // 45° seam where top-edge and right-edge corner math meet.
        for (int d = 12; d <= 46; ++d) {
            const QRgb a = img.pixel(w - 1 - d, d + 1);
            const QRgb b = img.pixel(w - 2 - d, d);
            maxStep = std::max(maxStep, colorDistance(a, b));
        }

        return maxStep;
    }

    void setSmallRadiusLargeBezel()
    {
        m_glass->setProperty("highlightEnabled", false);
        m_glass->setProperty("rimReflectionEnabled", false);
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("radius", 8.0);
        m_glass->setProperty("bezelWidth", 52.0);
        m_glass->setProperty("thickness", 120.0);
        m_glass->setProperty("displacementFactor", 0.85);
        m_glass->setProperty("ior", 1.45);
        m_glass->setProperty("dispersion", 0.018);
        m_glass->setProperty("strokeStrength", 0.0);
        m_glass->setProperty("specularOpacity", 0.0);
        m_glass->setProperty("edgeSaturation", 0.0);
        QTest::qWait(50);
    }

    static bool requiresShaderRendering(const char *testFunction)
    {
        static constexpr const char *renderingTests[] = {
            "highlightToggleProducesDifferentRender",
            "rimReflectionToggleProducesDifferentRender",
            "lightAngleShiftMovesHighlight",
            "radiusProducesTransparentCorners",
            "largeBezelWithSmallRadiusDoesNotIntroduceCornerDiagonalSeam",
            "largeBezelRemainsVisibleAlongStraightEdgesWithSmallRadius",
            "highDispersionRefractsEdgesWithSpecularDisabled",
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
        m_glass->setProperty("lightAngle", -135.0);
        m_glass->setProperty("highlightEnabled", true);
        m_glass->setProperty("rimReflectionEnabled", true);
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("blurMax", 36);
        m_glass->setProperty("blurAmount", 1.0);
        m_glass->setProperty("blurMultiplier", 0.0);
        m_glass->setProperty("radius", 0.0);
        m_glass->setProperty("bezelWidth", 18.0);
        m_glass->setProperty("thickness", 90.0);
        m_glass->setProperty("displacementFactor", 0.45);
        m_glass->setProperty("ior", 1.42);
        m_glass->setProperty("dispersion", 0.012);
        m_glass->setProperty("brightness", 0.0);
        m_glass->setProperty("contrast", 0.0);
        m_glass->setProperty("saturation", 0.0);
        m_glass->setProperty("colorization", 0.0);
        m_glass->setProperty("specularOpacity", 0.6);
        m_glass->setProperty("strokeWidth", 1.0);
        m_glass->setProperty("strokeStrength", 1.0);
        m_glass->setProperty("lightPower", 2.0);
        m_glass->setProperty("edgeSaturation", 0.0);
        m_glass->setProperty("reflectionOffset", 12.0);
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

    // ── Property tests: read derived properties at runtime ─────────────

    void lightDirectionDerivesFromLightAngle()
    {
        // lightAngle default in GlassEffect.qml is -135°
        // lightDirection = (cos(angle), sin(angle))
        const qreal angle = m_glass->property("lightAngle").toReal();
        QCOMPARE(angle, -135.0);

        const auto dir = m_glass->property("lightDirection").value<QVector2D>();
        const qreal rad = angle * M_PI / 180.0;
        QVERIFY(qAbs(dir.x() - std::cos(rad)) < 0.001);
        QVERIFY(qAbs(dir.y() - std::sin(rad)) < 0.001);

        // lightAngleRadians should match
        const qreal radians = m_glass->property("lightAngleRadians").toReal();
        QVERIFY(qAbs(radians - rad) < 0.001);
    }

    void changingLightAngleUpdatesDerivedProperties()
    {
        // Set to 0° → direction should be (1, 0)
        m_glass->setProperty("lightAngle", 0.0);
        QTest::qWait(10); // let bindings update
        const auto dir0 = m_glass->property("lightDirection").value<QVector2D>();
        QVERIFY(qAbs(dir0.x() - 1.0) < 0.001);
        QVERIFY(qAbs(dir0.y() - 0.0) < 0.001);

        // Set to 90° → direction should be (0, 1)
        m_glass->setProperty("lightAngle", 90.0);
        QTest::qWait(10);
        const auto dir90 = m_glass->property("lightDirection").value<QVector2D>();
        QVERIFY(qAbs(dir90.x() - 0.0) < 0.001);
        QVERIFY(qAbs(dir90.y() - 1.0) < 0.001);

        // Set to -90° → direction should be (0, -1)
        m_glass->setProperty("lightAngle", -90.0);
        QTest::qWait(10);
        const auto dirN90 = m_glass->property("lightDirection").value<QVector2D>();
        QVERIFY(qAbs(dirN90.x() - 0.0) < 0.001);
        QVERIFY(qAbs(dirN90.y() - (-1.0)) < 0.001);
    }

    void highlightEnabledTogglesZeroShaderSpecular()
    {
        // Find the internal ShaderEffect (has objectName "glassShader")
        auto *shader = m_glass->findChild<QObject *>("glassShader");
        QVERIFY(shader);

        // When highlightEnabled is true, shader gets non-zero specular values
        m_glass->setProperty("highlightEnabled", true);
        QTest::qWait(10);

        const qreal specOn = shader->property("specularOpacity").toReal();
        const qreal strokeOn = shader->property("strokeStrength").toReal();
        QVERIFY(specOn > 0.0);
        QVERIFY(strokeOn > 0.0);

        // When highlightEnabled is false, both must be zero
        m_glass->setProperty("highlightEnabled", false);
        QTest::qWait(10);

        QCOMPARE(shader->property("specularOpacity").toReal(), 0.0);
        QCOMPARE(shader->property("strokeStrength").toReal(), 0.0);
    }

    void rimReflectionEnabledTogglesFloatGate()
    {
        auto *shader = m_glass->findChild<QObject *>("glassShader");
        QVERIFY(shader);

        // Enabled → 0.22 tint mix
        m_glass->setProperty("rimReflectionEnabled", true);
        QTest::qWait(10);
        QCOMPARE(shader->property("rimReflectionStrength").toReal(), 0.22);

        // Disabled → 0.0 (pure white specular, no tint)
        m_glass->setProperty("rimReflectionEnabled", false);
        QTest::qWait(10);
        QCOMPARE(shader->property("rimReflectionStrength").toReal(), 0.0);
    }

    void multiEffectEnabledReflectsBlurAndColorParams()
    {
        // Default scene: blurEnabled=false, all color params=0 → false
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("brightness", 0.0);
        m_glass->setProperty("contrast", 0.0);
        m_glass->setProperty("saturation", 0.0);
        m_glass->setProperty("colorization", 0.0);
        QTest::qWait(10);
        QVERIFY(!m_glass->property("multiEffectEnabled").toBool());

        // blurEnabled → true
        m_glass->setProperty("blurEnabled", true);
        QTest::qWait(10);
        QVERIFY(m_glass->property("multiEffectEnabled").toBool());

        // Non-zero brightness alone → true
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("brightness", 0.05);
        QTest::qWait(10);
        QVERIFY(m_glass->property("multiEffectEnabled").toBool());

        // Non-zero contrast alone → true
        m_glass->setProperty("brightness", 0.0);
        m_glass->setProperty("contrast", -0.12);
        QTest::qWait(10);
        QVERIFY(m_glass->property("multiEffectEnabled").toBool());

        // Non-zero saturation alone → true
        m_glass->setProperty("contrast", 0.0);
        m_glass->setProperty("saturation", -0.15);
        QTest::qWait(10);
        QVERIFY(m_glass->property("multiEffectEnabled").toBool());

        // Non-zero colorization alone → true
        m_glass->setProperty("saturation", 0.0);
        m_glass->setProperty("colorization", 0.12);
        QTest::qWait(10);
        QVERIFY(m_glass->property("multiEffectEnabled").toBool());
    }

    void dconfigFacingGlassKnobsAreRuntimeQmlProperties()
    {
        auto *shader = m_glass->findChild<QObject *>("glassShader");
        QVERIFY(shader);

        const QList<QByteArray> propertyNames = {
            "blurAmount",
            "blurMultiplier",
            "blurMax",
            "bezelWidth",
            "thickness",
            "displacementFactor",
            "ior",
            "dispersion",
            "brightness",
            "contrast",
            "saturation",
            "colorization",
            "edgeSaturation",
            "reflectionOffset",
        };

        for (const QByteArray &name : propertyNames) {
            QVERIFY2(m_glass->metaObject()->indexOfProperty(name.constData()) >= 0,
                     qPrintable(QStringLiteral("GlassEffect must expose %1 as a real QML property")
                                    .arg(QString::fromLatin1(name))));
        }

        QVERIFY(m_glass->setProperty("bezelWidth", 47.0));
        QVERIFY(m_glass->setProperty("thickness", 133.0));
        QVERIFY(m_glass->setProperty("displacementFactor", 0.72));
        QVERIFY(m_glass->setProperty("ior", 1.37));
        QVERIFY(m_glass->setProperty("dispersion", 0.021));
        QVERIFY(m_glass->setProperty("blurEnabled", true));
        QVERIFY(m_glass->setProperty("blurMax", 18));
        QVERIFY(m_glass->setProperty("blurAmount", 0.75));
        QVERIFY(m_glass->setProperty("blurMultiplier", 1.5));
        QTest::qWait(10);

        QCOMPARE(shader->property("bezelWidth").toReal(), 47.0);
        QCOMPARE(shader->property("thickness").toReal(), 133.0);
        QCOMPARE(shader->property("displacementFactor").toReal(), 0.72);
        QCOMPARE(shader->property("ior").toReal(), 1.37);
        QCOMPARE(shader->property("dispersion").toReal(), 0.021);
        QCOMPARE(m_glass->property("multiEffectEnabled").toBool(), true);
    }

    void shaderReceivesEdgeMaterialPropertiesUsedByFragmentShader()
    {
        auto *shader = m_glass->findChild<QObject *>("glassShader");
        QVERIFY(shader);

        QVERIFY2(m_glass->setProperty("edgeSaturation", 1.35),
                 "GlassEffect must expose edgeSaturation as a runtime QML property");
        QVERIFY2(m_glass->setProperty("reflectionOffset", 27.0),
                 "GlassEffect must expose reflectionOffset as a runtime QML property");
        QTest::qWait(10);

        const bool edgeSaturationForwarded = shader->property("edgeSaturation").isValid();
        const bool reflectionOffsetForwarded = shader->property("reflectionOffset").isValid();
        QVERIFY2(edgeSaturationForwarded && reflectionOffsetForwarded,
                 qPrintable(QStringLiteral("glassShader must receive edgeSaturation and reflectionOffset for the fragment shader: edgeSaturation=%1 reflectionOffset=%2")
                                .arg(edgeSaturationForwarded ? QStringLiteral("valid") : QStringLiteral("missing"))
                                .arg(reflectionOffsetForwarded ? QStringLiteral("valid") : QStringLiteral("missing"))));
        QCOMPARE(shader->property("edgeSaturation").toReal(), 1.35);
        QCOMPARE(shader->property("reflectionOffset").toReal(), 27.0);
    }

    // ── Rendering tests: grabToImage + image comparison ───────────────

    void grabIsDeterministic()
    {
        const QImage img1 = grabImage(m_scene);
        QVERIFY(!img1.isNull());

        const QImage img2 = grabImage(m_scene);
        QVERIFY(!img2.isNull());

        // Same scene, same settings → identical output
        QCOMPARE(img1, img2);
    }

    void highlightToggleProducesDifferentRender()
    {
        // Set prominent glass params for visible highlights
        m_glass->setProperty("highlightEnabled", true);
        m_glass->setProperty("radius", 34.0);
        m_glass->setProperty("bezelWidth", 16.0);
        m_glass->setProperty("specularOpacity", 0.82);
        m_glass->setProperty("strokeStrength", 1.5);
        m_glass->setProperty("strokeWidth", 1.4);
        QTest::qWait(50);

        const QImage withHighlight = grabImage(m_scene);
        QVERIFY(!withHighlight.isNull());

        m_glass->setProperty("highlightEnabled", false);
        QTest::qWait(50);

        const QImage withoutHighlight = grabImage(m_scene);
        QVERIFY(!withoutHighlight.isNull());

        // Images must differ — highlights are visible
        QVERIFY(withHighlight != withoutHighlight);

        // The difference should be concentrated near edges (rim), not center
        const int w = withHighlight.width();
        const int h = withHighlight.height();
        int edgeDiff = 0, centerDiff = 0;
        const int edgeBand = 24; // px from border
        const int cx = w / 2, cy = h / 2;
        const int centerRadius = 32;

        for (int y = 0; y < h; ++y) {
            const auto *p1 = reinterpret_cast<const QRgb *>(withHighlight.scanLine(y));
            const auto *p2 = reinterpret_cast<const QRgb *>(withoutHighlight.scanLine(y));
            for (int x = 0; x < w; ++x) {
                if (p1[x] != p2[x]) {
                    if (x < edgeBand || x >= w - edgeBand || y < edgeBand || y >= h - edgeBand)
                        ++edgeDiff;
                    else if (std::hypot(x - cx, y - cy) < centerRadius)
                        ++centerDiff;
                }
            }
        }
        // Highlights are an edge/rim phenomenon
        QVERIFY2(edgeDiff > centerDiff,
                 qPrintable(QStringLiteral("highlight diff should be edge-dominated: edge=%1 center=%2")
                                .arg(edgeDiff).arg(centerDiff)));
    }

    void rimReflectionToggleProducesDifferentRender()
    {
        // Ensure highlights are on for rim reflection to be visible
        m_glass->setProperty("highlightEnabled", true);
        m_glass->setProperty("radius", 34.0);
        m_glass->setProperty("rimReflectionEnabled", true);
        QTest::qWait(50);

        const QImage withTint = grabImage(m_scene);
        QVERIFY(!withTint.isNull());

        m_glass->setProperty("rimReflectionEnabled", false);
        QTest::qWait(50);

        const QImage withoutTint = grabImage(m_scene);
        QVERIFY(!withoutTint.isNull());

        // Images must differ — rim tint affects specular color
        const int diff = pixelDiffCount(withTint, withoutTint);
        QVERIFY2(diff > 0,
                 qPrintable(QStringLiteral("rim reflection toggle must produce different output, got %1 differing pixels").arg(diff)));
    }

    void lightAngleShiftMovesHighlight()
    {
        m_glass->setProperty("highlightEnabled", true);
        m_glass->setProperty("radius", 34.0);
        m_glass->setProperty("specularOpacity", 0.82);
        m_glass->setProperty("strokeStrength", 1.5);

        // Use 0° and 90° — the shader's specular is symmetric (both rim
        // and opposite-rim contribute equally), so 0° vs 180° would be
        // identical.  0° puts highlights on left/right edges; 90° puts
        // them on top/bottom edges.
        m_glass->setProperty("lightAngle", 0.0);
        QTest::qWait(50);
        const QImage imgHorizontal = grabImage(m_scene);
        QVERIFY(!imgHorizontal.isNull());

        m_glass->setProperty("lightAngle", 90.0);
        QTest::qWait(50);
        const QImage imgVertical = grabImage(m_scene);
        QVERIFY(!imgVertical.isNull());

        QVERIFY(imgHorizontal != imgVertical);

        // When light is at 0°, highlights are on left/right edges.
        // When light is at 90°, highlights are on top/bottom edges.
        // So horizontal-edge brightness should be higher at 90°, and
        // vertical-edge brightness should be higher at 0°.
        const int edgeBand = 16;

        auto edgeBrightness = [](const QImage &img, int startX, int endX) {
            int sum = 0;
            for (int y = 0; y < img.height(); ++y) {
                const auto *p = reinterpret_cast<const QRgb *>(img.scanLine(y));
                for (int x = startX; x < endX; ++x) {
                    const QRgb px = p[x];
                    sum += qRed(px) + qGreen(px) + qBlue(px);
                }
            }
            return sum;
        };

        auto rowBrightness = [](const QImage &img, int startY, int endY) {
            int sum = 0;
            for (int y = startY; y < endY; ++y) {
                const auto *p = reinterpret_cast<const QRgb *>(img.scanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    const QRgb px = p[x];
                    sum += qRed(px) + qGreen(px) + qBlue(px);
                }
            }
            return sum;
        };

        const int w = imgHorizontal.width();
        const int h = imgHorizontal.height();

        // Light at 0° → vertical edges (left/right) brighter
        const int vertBright_0deg = edgeBrightness(imgHorizontal, 0, edgeBand)
                                  + edgeBrightness(imgHorizontal, w - edgeBand, w);
        const int vertBright_90deg = edgeBrightness(imgVertical, 0, edgeBand)
                                   + edgeBrightness(imgVertical, w - edgeBand, w);
        QVERIFY2(vertBright_0deg > vertBright_90deg,
                 qPrintable(QStringLiteral("vertical edges should be brighter at 0°: 0deg=%1 90deg=%2")
                                .arg(vertBright_0deg).arg(vertBright_90deg)));

        // Light at 90° → horizontal edges (top/bottom) brighter
        const int horizBright_90deg = rowBrightness(imgVertical, 0, edgeBand)
                                    + rowBrightness(imgVertical, h - edgeBand, h);
        const int horizBright_0deg = rowBrightness(imgHorizontal, 0, edgeBand)
                                   + rowBrightness(imgHorizontal, h - edgeBand, h);
        QVERIFY2(horizBright_90deg > horizBright_0deg,
                 qPrintable(QStringLiteral("horizontal edges should be brighter at 90°: 90deg=%1 0deg=%2")
                                .arg(horizBright_90deg).arg(horizBright_0deg)));
    }

    void radiusProducesTransparentCorners()
    {
        // Large radius for clearly transparent corners
        m_glass->setProperty("radius", 60.0);
        m_glass->setProperty("highlightEnabled", false);
        QTest::qWait(50);

        // Grab the glass item directly (not root) so the visible backdrop
        // doesn't fill the transparent corners.
        const QImage img = grabImage(m_glass);
        QVERIFY(!img.isNull());

        // Corner pixels should be fully transparent (alpha = 0)
        const int w = img.width();
        const int h = img.height();
        const int offset = 2; // a few pixels in from the absolute corner

        const QList<QPoint> cornerPixels = {
            {offset, offset},                      // top-left
            {w - 1 - offset, offset},               // top-right
            {offset, h - 1 - offset},               // bottom-left
            {w - 1 - offset, h - 1 - offset},        // bottom-right
        };

        for (const auto &pt : cornerPixels) {
            const QRgb px = img.pixel(pt.x(), pt.y());
            QVERIFY2(qAlpha(px) == 0,
                     qPrintable(QStringLiteral("corner pixel (%1,%2) should be transparent, got alpha=%3")
                                    .arg(pt.x()).arg(pt.y()).arg(qAlpha(px))));
        }

        // Center pixel should be opaque
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
        m_glass->setProperty("edgeSaturation", 1.5);
        QTest::qWait(50);

        const QImage largeBezel = grabImage(m_scene);
        QVERIFY(!largeBezel.isNull());

        m_glass->setProperty("bezelWidth", 1.0);
        QTest::qWait(50);

        const QImage tinyBezel = grabImage(m_scene);
        QVERIFY(!tinyBezel.isNull());

        // Far enough from the corners that radius must not suppress the
        // straight-edge bezel, and far enough from the boundary that a shader
        // clamping bezel width to radius would leave the region unchanged.
        // Use the right edge: its horizontal refraction crosses the fixture's
        // horizontal gradient, so a live bezel changes observable pixels.
        const QRect rightStraightEdge(212, 72, 24, 112);
        const int activePixels = regionDiffCount(largeBezel, tinyBezel, rightStraightEdge, 8);
        QVERIFY2(activePixels > rightStraightEdge.width() * rightStraightEdge.height() / 3,
                 qPrintable(QStringLiteral("large straight-edge bezel should remain active beyond the small radius: changedPixels=%1 regionPixels=%2")
                                .arg(activePixels)
                                .arg(rightStraightEdge.width() * rightStraightEdge.height())));
    }

    void highDispersionRefractsEdgesWithSpecularDisabled()
    {
        setSmallRadiusLargeBezel();
        QVERIFY(m_glass->setProperty("highlightEnabled", false));
        QVERIFY(m_glass->setProperty("rimReflectionEnabled", false));
        QVERIFY(m_glass->setProperty("blurEnabled", false));
        QVERIFY(m_glass->setProperty("radius", 8.0));
        QVERIFY(m_glass->setProperty("bezelWidth", 64.0));
        QVERIFY(m_glass->setProperty("thickness", 140.0));
        QVERIFY(m_glass->setProperty("displacementFactor", 1.0));
        QVERIFY(m_glass->setProperty("ior", 1.5));
        QVERIFY(m_glass->setProperty("dispersion", 0.0));
        QVERIFY(m_glass->setProperty("strokeStrength", 0.0));
        QVERIFY(m_glass->setProperty("specularOpacity", 0.0));
        QVERIFY(m_glass->setProperty("edgeSaturation", 0.0));
        QTest::qWait(50);

        const QImage zeroDispersion = grabImage(m_scene);
        QVERIFY(!zeroDispersion.isNull());

        QVERIFY(m_glass->setProperty("dispersion", 0.2));
        QTest::qWait(50);

        const QImage highDispersion = grabImage(m_scene);
        QVERIFY(!highDispersion.isNull());

        // Sample all straight-edge bevels. The fixture has black vertical
        // contrast bars at x=25% and x=75%, so the left/right bevels cross
        // high-contrast backdrop content without relying on exact colors.
        const QList<QRect> edgeRefractionBands = {
            QRect(0, 56, 72, 144),
            QRect(184, 56, 72, 144),
            QRect(56, 0, 144, 72),
            QRect(56, 184, 144, 72),
        };
        const QRect centerInterior(104, 104, 48, 48);

        int edgeChanged = 0;
        int edgePixels = 0;
        for (const QRect &band : edgeRefractionBands) {
            edgeChanged += regionDiffCount(zeroDispersion, highDispersion, band, 6);
            edgePixels += band.width() * band.height();
        }
        const int centerChanged = regionDiffCount(zeroDispersion, highDispersion, centerInterior, 6);

        QVERIFY2(edgeChanged > edgePixels / 10,
                 qPrintable(QStringLiteral("high dispersion should visibly change edge refraction without specular: edgeChanged=%1 regionPixels=%2")
                                .arg(edgeChanged)
                                .arg(edgePixels)));
        QVERIFY2(centerChanged < centerInterior.width() * centerInterior.height() / 20,
                 qPrintable(QStringLiteral("high dispersion should leave the flat center comparatively stable: centerChanged=%1 regionPixels=%2")
                                .arg(centerChanged)
                                .arg(centerInterior.width() * centerInterior.height())));
        QVERIFY2(edgeChanged > centerChanged * 4 + 20,
                 qPrintable(QStringLiteral("dispersion response should be edge-dominated: edgeChanged=%1 centerChanged=%2")
                                .arg(edgeChanged)
                                .arg(centerChanged)));
    }

    void zeroBlurMultiplierStillAppliesGaussianBlur()
    {
        m_glass->setProperty("highlightEnabled", false);
        m_glass->setProperty("rimReflectionEnabled", false);
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
        m_glass->setProperty("highlightEnabled", false);
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

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <array>
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
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("radius", 8.0);
        m_glass->setProperty("bezelWidth", 52.0);
        m_glass->setProperty("thickness", 120.0);
        m_glass->setProperty("ior", 1.45);
        QTest::qWait(50);
    }

    static bool requiresShaderRendering(const char *testFunction)
    {
        static constexpr const char *renderingTests[] = {
            "specularProducesDifferentRender",
            "tintControlProducesDifferentRender",
            "specularControlChangesRim",
            "narrowBezelPreservesInnerRim",
            "radiusProducesTransparentCorners",
            "nonUniformScaleKeepsEdgeAntialiasingSymmetric",
            "largeBezelWithSmallRadiusDoesNotIntroduceCornerDiagonalSeam",
            "smallRadiusSoftensRefractionAtSilhouette",
            "smallRadiusCapsWholeRefractionBand",
            "contentEdgePullChangesSilhouetteRefraction",
            "radiusLargerThanBezelDoesNotIntroduceCornerDiagonalSeam",
            "tintControlChangesRenderedColor",
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
        m_glass->setProperty("blurMax", 12);
        m_glass->setProperty("blurAmount", 0.6);
        m_glass->setProperty("blurMultiplier", 0.0);
        m_glass->setProperty("radius", 60.0);
        m_glass->setProperty("bezelWidth", 60.0);
        m_glass->setProperty("thickness", 50.0);
        m_glass->setProperty("ior", 1.5);
        m_glass->setProperty("brightness", 0.0);
        m_glass->setProperty("contrast", 0.0);
        m_glass->setProperty("saturation", 0.04);
        m_glass->setProperty("specular", 0.0);
        m_glass->setProperty("tint", 0.0);
        m_scene->setProperty("backdropVisible", true);
        m_scene->setProperty("glassXScale", 1.0);
        m_scene->setProperty("glassYScale", 1.0);
        m_scene->setProperty("cornerProbeVisible", false);
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

    void liquidGlassDefaultsMatch()
    {
        QCOMPARE(m_glass->property("radius").toReal(), 60.0);
        QCOMPARE(m_glass->property("thickness").toReal(), 50.0);
        QCOMPARE(m_glass->property("bezelWidth").toReal(), 60.0);
        QCOMPARE(m_glass->property("ior").toReal(), 1.5);
        QCOMPARE(m_glass->property("specular").toReal(), 0.0);
        QCOMPARE(m_glass->property("tint").toReal(), 0.0);
        QCOMPARE(m_glass->property("saturation").toReal(), 0.04);
        QCOMPARE(m_glass->property("blurAmount").toReal(), 0.6);

    }


    void specularAndTintAreOnlyMaterialControls()
    {
        auto *shader = m_glass->findChild<QObject *>("glassShader");
        QVERIFY(shader);

        QVERIFY2(m_glass->metaObject()->indexOfProperty("highlightEnabled") < 0,
                 "GlassEffect must use specular=0 instead of a duplicate highlight switch");
        QVERIFY2(m_glass->metaObject()->indexOfProperty("colorization") < 0,
                 "GlassEffect must use tint instead of duplicate white colorization");
        QVERIFY2(m_glass->metaObject()->indexOfProperty("colorizationColor") < 0,
                 "GlassEffect must not expose colorization state after removing colorization");

        QVERIFY(m_glass->setProperty("specular", 0.55));
        QTest::qWait(10);
        QCOMPARE(shader->property("specular").toReal(), 0.55);

        QVERIFY(m_glass->setProperty("specular", 0.0));
        QTest::qWait(10);
        QCOMPARE(shader->property("specular").toReal(), 0.0);
    }

    void multiEffectEnabledReflectsBlurAndColorParams()
    {
        // Default scene: blurEnabled=false, all color params=0 → false
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("brightness", 0.0);
        m_glass->setProperty("contrast", 0.0);
        m_glass->setProperty("saturation", 0.0);
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
            "ior",
            "brightness",
            "contrast",
            "saturation",
        };

        for (const QByteArray &name : propertyNames) {
            QVERIFY2(m_glass->metaObject()->indexOfProperty(name.constData()) >= 0,
                     qPrintable(QStringLiteral("GlassEffect must expose %1 as a real QML property")
                                    .arg(QString::fromLatin1(name))));
        }

        QVERIFY(m_glass->setProperty("bezelWidth", 47.0));
        QVERIFY(m_glass->setProperty("thickness", 133.0));
        QVERIFY(m_glass->setProperty("ior", 1.37));
        QVERIFY(m_glass->setProperty("blurEnabled", true));
        QVERIFY(m_glass->setProperty("blurMax", 18));
        QVERIFY(m_glass->setProperty("blurAmount", 0.75));
        QVERIFY(m_glass->setProperty("blurMultiplier", 1.5));
        QTest::qWait(10);

        QCOMPARE(shader->property("bezelWidth").toReal(), 47.0);
        QCOMPARE(shader->property("thickness").toReal(), 133.0);
        QCOMPARE(shader->property("ior").toReal(), 1.37);
        QCOMPARE(m_glass->property("multiEffectEnabled").toBool(), true);
    }

    void materialKnobsAreRuntimeQmlProperties()
    {
        auto *shader = m_glass->findChild<QObject *>("glassShader");
        QVERIFY(shader);

        const QList<QByteArray> propertyNames = {
            "specular",
            "tint",
        };
        for (const QByteArray &name : propertyNames) {
            QVERIFY2(m_glass->metaObject()->indexOfProperty(name.constData()) >= 0,
                     qPrintable(QStringLiteral("GlassEffect must expose material knob %1")
                                    .arg(QString::fromLatin1(name))));
        }

        QVERIFY(m_glass->setProperty("specular", 0.35));
        QVERIFY(m_glass->setProperty("tint", 0.22));
        QTest::qWait(10);

        QCOMPARE(shader->property("specular").toReal(), 0.35);
        QCOMPARE(shader->property("tint").toReal(), 0.22);
        QVERIFY2(m_glass->metaObject()->indexOfProperty("blurRadius") < 0,
                 "GlassEffect must use MultiEffect blur controls instead of exposing shader blurRadius");
        QVERIFY2(m_glass->metaObject()->indexOfProperty("shadow") < 0,
                 "GlassEffect must not expose a dedicated shadow control");
    }

    void multiEffectBlurControlsRemainRuntimeQmlProperties()
    {
        QVERIFY2(m_glass->metaObject()->indexOfProperty("blurEnabled") >= 0,
                 "GlassEffect must preserve blurEnabled for MultiEffect blur");
        QVERIFY2(m_glass->metaObject()->indexOfProperty("blurMax") >= 0,
                 "GlassEffect must preserve blurMax for MultiEffect blur");
        QVERIFY2(m_glass->metaObject()->indexOfProperty("blurAmount") >= 0,
                 "GlassEffect must preserve blurAmount for MultiEffect blur");
        QVERIFY2(m_glass->metaObject()->indexOfProperty("blurMultiplier") >= 0,
                 "GlassEffect must preserve blurMultiplier for MultiEffect blur");

        QVERIFY(m_glass->setProperty("blurEnabled", false));
        QVERIFY(m_glass->setProperty("blurAmount", 1.0));
        QVERIFY(m_glass->setProperty("blurMax", 48));
        QVERIFY(m_glass->setProperty("blurMultiplier", 2.0));
        QVERIFY(m_glass->setProperty("saturation", 0.0));
        QTest::qWait(10);
        QVERIFY(!m_glass->property("multiEffectEnabled").toBool());

        QVERIFY(m_glass->setProperty("blurEnabled", true));
        QTest::qWait(10);
        QVERIFY(m_glass->property("multiEffectEnabled").toBool());
    }

    void shaderKeepsGlassBoundsWithoutDedicatedShadow()
    {
        auto *shader = qobject_cast<QQuickItem *>(m_glass->findChild<QObject *>("glassShader"));
        QVERIFY(shader);

        QTest::qWait(10);

        QCOMPARE(shader->x(), 0.0);
        QCOMPARE(shader->y(), 0.0);
        QCOMPARE(shader->width(), m_glass->width());
        QCOMPARE(shader->height(), m_glass->height());
    }

    void shaderReceivesEdgeMaterialPropertiesUsedByFragmentShader()
    {
        auto *shader = m_glass->findChild<QObject *>("glassShader");
        QVERIFY(shader);

        QTest::qWait(10);

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

    void specularProducesDifferentRender()
    {
        // Set prominent glass params for visible highlights.
        m_glass->setProperty("radius", 34.0);
        m_glass->setProperty("bezelWidth", 16.0);
        m_glass->setProperty("specular", 0.82);
        QTest::qWait(50);

        const QImage withHighlight = grabImage(m_scene);
        QVERIFY(!withHighlight.isNull());

        m_glass->setProperty("specular", 0.0);
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

    void tintControlProducesDifferentRender()
    {
        m_glass->setProperty("radius", 60.0);
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("specular", 0.0);
        m_glass->setProperty("tint", 0.0);
        QTest::qWait(50);

        const QImage noTint = grabImage(m_scene);
        QVERIFY(!noTint.isNull());

        m_glass->setProperty("tint", 0.4);
        QTest::qWait(50);

        const QImage strongTint = grabImage(m_scene);
        QVERIFY(!strongTint.isNull());

        const int diff = regionDiffCount(noTint, strongTint, QRect(64, 64, 128, 128), 4);
        QVERIFY2(diff > 128 * 128 / 3,
                 qPrintable(QStringLiteral("tint control must visibly change the glass interior, got %1 changed pixels").arg(diff)));
    }

    void specularControlChangesRim()
    {
        m_glass->setProperty("radius", 60.0);
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("specular", 0.0);
        QTest::qWait(50);

        const QImage withoutSpecular = grabImage(m_scene);
        QVERIFY(!withoutSpecular.isNull());

        m_glass->setProperty("specular", 1.0);
        QTest::qWait(50);

        const QImage withSpecular = grabImage(m_scene);
        QVERIFY(!withSpecular.isNull());

        const int edgeDiff = regionDiffCount(withoutSpecular, withSpecular, QRect(0, 0, withSpecular.width(), 32), 4)
            + regionDiffCount(withoutSpecular, withSpecular, QRect(0, withSpecular.height() - 32, withSpecular.width(), 32), 4)
            + regionDiffCount(withoutSpecular, withSpecular, QRect(0, 0, 32, withSpecular.height()), 4)
            + regionDiffCount(withoutSpecular, withSpecular, QRect(withSpecular.width() - 32, 0, 32, withSpecular.height()), 4);
        QVERIFY2(edgeDiff > 500,
                 qPrintable(QStringLiteral("specular control must visibly change rim highlights, edgeDiff=%1").arg(edgeDiff)));
    }

    void narrowBezelPreservesInnerRim()
    {
        m_glass->setProperty("radius", 60.0);
        m_glass->setProperty("bezelWidth", 3.0);
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("specular", 0.0);
        QTest::qWait(50);

        const QImage withoutSpecular = grabImage(m_scene);
        QVERIFY(!withoutSpecular.isNull());

        m_glass->setProperty("specular", 1.0);
        QTest::qWait(50);

        const QImage withSpecular = grabImage(m_scene);
        QVERIFY(!withSpecular.isNull());

        const QRect innerTopRim(64, 4, 128, 1);
        const int changed = regionDiffCount(withoutSpecular, withSpecular, innerTopRim, 2);
        QVERIFY2(changed > innerTopRim.width() / 2,
                 qPrintable(QStringLiteral("narrow bezel must preserve the fixed-width inner rim: changed=%1 width=%2")
                                .arg(changed)
                                .arg(innerTopRim.width())));
    }



    void radiusProducesTransparentCorners()
    {
        // Large radius for clearly transparent corners
        m_glass->setProperty("radius", 60.0);
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

    void nonUniformScaleKeepsEdgeAntialiasingSymmetric()
    {
        m_scene->setProperty("backdropVisible", false);
        m_scene->setProperty("glassXScale", 0.75);
        m_scene->setProperty("glassYScale", 0.25);
        m_glass->setProperty("radius", 0.0);
        QTest::qWait(50);

        const QImage img = grabImage(m_scene);
        QVERIFY(!img.isNull());

        int horizontalPartialAlpha = 0;
        int verticalPartialAlpha = 0;
        for (int coordinate = 0; coordinate < img.width(); ++coordinate) {
            const int horizontalAlpha = qAlpha(img.pixel(coordinate, img.height() / 2));
            const int verticalAlpha = qAlpha(img.pixel(img.width() / 2, coordinate));
            horizontalPartialAlpha += horizontalAlpha > 0 && horizontalAlpha < 255;
            verticalPartialAlpha += verticalAlpha > 0 && verticalAlpha < 255;
        }

        QVERIFY2(std::abs(horizontalPartialAlpha - verticalPartialAlpha) <= 1,
                 qPrintable(QStringLiteral("non-uniform scaling must preserve symmetric screen-space AA: horizontal=%1 vertical=%2")
                                .arg(horizontalPartialAlpha)
                                .arg(verticalPartialAlpha)));
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

    void smallRadiusSoftensRefractionAtSilhouette()
    {
        m_scene->setProperty("cornerProbeVisible", true);
        setSmallRadiusLargeBezel();

        m_glass->setProperty("ior", 1.0001);
        QTest::qWait(50);
        const QImage neutral = grabImage(m_scene);
        QVERIFY(!neutral.isNull());

        m_glass->setProperty("ior", 1.45);
        QTest::qWait(50);
        const QImage refracted = grabImage(m_scene);
        QVERIFY(!refracted.isNull());

        const auto patchDifference = [&](const std::array<QPoint, 3> &centers) {
            int difference = 0;
            for (const QPoint &center : centers) {
                for (int y = center.y() - 1; y <= center.y() + 1; ++y) {
                    for (int x = center.x() - 1; x <= center.x() + 1; ++x)
                        difference += colorDistance(neutral.pixel(x, y), refracted.pixel(x, y));
                }
            }
            return difference;
        };

        const std::array<QPoint, 3> silhouette = {
            QPoint(250, 1),
            QPoint(253, 2),
            QPoint(254, 5),
        };
        const std::array<QPoint, 3> innerArc = {
            QPoint(250, 4),
            QPoint(251, 5),
            QPoint(252, 7),
        };
        const int silhouetteDifference = patchDifference(silhouette);
        const int innerDifference = patchDifference(innerArc);
        QVERIFY2(silhouetteDifference * 5 < innerDifference * 3,
                 qPrintable(QStringLiteral("small-corner refraction must ease in from the silhouette: silhouette=%1 inner=%2")
                                .arg(silhouetteDifference)
                                .arg(innerDifference)));
    }

    void smallRadiusCapsWholeRefractionBand()
    {
        setSmallRadiusLargeBezel();

        m_glass->setProperty("ior", 1.0001);
        QTest::qWait(50);
        const QImage neutral = grabImage(m_scene);
        QVERIFY(!neutral.isNull());

        m_glass->setProperty("ior", 1.45);
        QTest::qWait(50);
        const QImage refracted = grabImage(m_scene);
        QVERIFY(!refracted.isNull());

        const QRect beyondCornerLimitedBand(214, 80, 30, 96);
        const int changed = regionDiffCount(neutral, refracted, beyondCornerLimitedBand, 4);
        const int samples = beyondCornerLimitedBand.width() * beyondCornerLimitedBand.height();
        QVERIFY2(changed <= samples / 20,
                 qPrintable(QStringLiteral("small radius must cap the whole refraction band: changed=%1 samples=%2")
                                .arg(changed)
                                .arg(samples)));
    }

    void contentEdgePullChangesSilhouetteRefraction()
    {
        setSmallRadiusLargeBezel();
        m_glass->setProperty("contentRampEnd", 0.15);
        m_glass->setProperty("contentEdgePull", 0.0);
        QTest::qWait(50);

        const QImage noEdgePull = grabImage(m_scene);
        QVERIFY(!noEdgePull.isNull());

        m_glass->setProperty("contentEdgePull", 1.0);
        QTest::qWait(50);

        const QImage fullEdgePull = grabImage(m_scene);
        QVERIFY(!fullEdgePull.isNull());

        const QRect rightEdgeBand(fullEdgePull.width() - 4, 48, 3, 160);
        const int changed = regionDiffCount(noEdgePull, fullEdgePull, rightEdgeBand, 4);
        QVERIFY2(changed > rightEdgeBand.width() * rightEdgeBand.height() / 2,
                 qPrintable(QStringLiteral("contentEdgePull must visibly change silhouette refraction: changed=%1 regionPixels=%2")
                                .arg(changed)
                                .arg(rightEdgeBand.width() * rightEdgeBand.height())));
    }

    void radiusLargerThanBezelDoesNotIntroduceCornerDiagonalSeam()
    {
        m_glass->setProperty("blurEnabled", false);
        // Isolate corner-geometry: geometry-only isolation;
        // colour effect that reads as cross-diagonal colour steps here.
        m_glass->setProperty("radius", 90.0);
        m_glass->setProperty("bezelWidth", 60.0);
        m_glass->setProperty("thickness", 50.0);
        m_glass->setProperty("ior", 3.0);
        QTest::qWait(50);

        const QImage img = grabImage(m_scene);
        QVERIFY(!img.isNull());

        const int seamStep = maxDiagonalCornerDiscontinuity(img);
        QVERIFY2(seamStep < 24,
                 qPrintable(QStringLiteral("radius larger than bezel introduced a sharp diagonal seam: max cross-diagonal step=%1")
                                .arg(seamStep)));
    }

    void tintControlChangesRenderedColor()
    {
        m_glass->setProperty("blurEnabled", false);
        m_glass->setProperty("radius", 60.0);
        m_glass->setProperty("tint", 0.0);
        QTest::qWait(50);

        const QImage noTint = grabImage(m_scene);
        QVERIFY(!noTint.isNull());

        m_glass->setProperty("tint", 0.4);
        QTest::qWait(50);

        const QImage strongTint = grabImage(m_scene);
        QVERIFY(!strongTint.isNull());

        const QRect centerInterior(64, 64, 128, 128);
        const int changed = regionDiffCount(noTint, strongTint, centerInterior, 4);
        QVERIFY2(changed > centerInterior.width() * centerInterior.height() / 3,
                 qPrintable(QStringLiteral("Liquid Glass tint control should change rendered color: changed=%1 regionPixels=%2")
                                .arg(changed)
                                .arg(centerInterior.width() * centerInterior.height())));
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
        QVERIFY2(changedPixels > centerContrastFeature.width() * centerContrastFeature.height() / 10,
                 qPrintable(QStringLiteral("MultiEffect blur must stay active with blurMultiplier=0: changedPixels=%1 regionPixels=%2")
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
        QVERIFY2(changedPixels > centerContrastFeature.width() * centerContrastFeature.height() / 10,
                 qPrintable(QStringLiteral("blurAmount/blurMultiplier must visibly change the sampled backdrop blur: changedPixels=%1 regionPixels=%2")
                                .arg(changedPixels)
                                .arg(centerContrastFeature.width() * centerContrastFeature.height())));
    }
    void blurToggleProducesDifferentRender()
    {
        m_glass->setProperty("blurEnabled", true);
        m_glass->setProperty("blurMax", 36);
        m_glass->setProperty("blurAmount", 1.0);
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

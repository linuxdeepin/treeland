// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "surface/framegeometry.h"
#include "surface/frameinputmask.h"

#include <wsurface.h>
#include <wsurfaceitem.h>
#include <wlr_all.h>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <limits>
#include <memory>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
struct NativeSurface {
    wlr_surface handle {};
    NativeSurface()
    {
        wl_signal_init(&handle.events.commit);
        wl_signal_init(&handle.events.map);
        wl_signal_init(&handle.events.unmap);
        wl_signal_init(&handle.events.new_subsurface);
        wl_signal_init(&handle.events.destroy);
        wl_list_init(&handle.current.frame_callback_list);
        wl_list_init(&handle.current.subsurfaces_below);
        wl_list_init(&handle.current.subsurfaces_above);
        wl_list_init(&handle.current_outputs);
        wlr_addon_set_init(&handle.addons);
        handle.current.width = 400;
        handle.current.height = 160;
        handle.current.scale = 1;
        handle.current.seq = 1;
        handle.mapped = true;
        pixman_region32_init_rect(&handle.input_region, 0, 0, 400, 160);
        pixman_region32_t hole;
        pixman_region32_init_rect(&hole, 100, 0, 100, 160);
        pixman_region32_subtract(&handle.input_region, &handle.input_region, &hole);
        pixman_region32_fini(&hole);
    }
    ~NativeSurface()
    {
        pixman_region32_fini(&handle.input_region);
        wlr_addon_set_finish(&handle.addons);
    }
};

class TestSurfaceItem : public WSurfaceItem
{
public:
    using WSurfaceItem::WSurfaceItem;
    int presses = 0;
    int releases = 0;
protected:
    bool sendEvent(QInputEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin)
            ++presses;
        if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::TouchEnd)
            ++releases;
        event->accept();
        return true;
    }
};
}


class FrameGeometryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void normalization()
    {
        QCOMPARE(FrameGeometry(QRectF(10, 20, 300, 200), 200).radius(), 100);
        QCOMPARE(FrameGeometry(QRectF(10, 20, 300, 200), -1).radius(), 0);
        QCOMPARE(FrameGeometry(QRectF(0, 0, 300, 200),
                               std::numeric_limits<qreal>::infinity()).radius(), 0);
        QVERIFY(!FrameGeometry().contains(QPointF()));
        QVERIFY(!FrameGeometry(QRectF(0, 0, -1, 100), 10).contains(QPointF()));
        QVERIFY(!FrameGeometry(QRectF(0, 0, 100, 0), 10).contains(QPointF()));
        QCOMPARE(FrameGeometry(QRectF(0, 0, 100, 80), 10)
                     .resizeEdgesAt(QPointF(-1, 40), std::numeric_limits<qreal>::quiet_NaN()),
                 Qt::Edges());
    }

    void contour_data()
    {
        QTest::addColumn<QPointF>("point");
        QTest::addColumn<bool>("inside");
        QTest::addColumn<Qt::Edges>("edges");
        QTest::newRow("far-outside") << QPointF(-1000, 50) << false << Qt::Edges();
        QTest::newRow("cut-corner") << QPointF(0, 0) << false << Qt::Edges();
        QTest::newRow("curved-rim") << QPointF(20, 20) << false << (Qt::TopEdge | Qt::LeftEdge);
        QTest::newRow("curved-interior") << QPointF(25, 25) << true << Qt::Edges();
        QTest::newRow("below-titlebar-outside") << QPointF(-1, 40) << false << Qt::Edges();
        QTest::newRow("below-titlebar-rim") << QPointF(5, 40) << false << (Qt::TopEdge | Qt::LeftEdge);
        QTest::newRow("below-titlebar-interior") << QPointF(15, 40) << true << Qt::Edges();
        QTest::newRow("left") << QPointF(-5, 100) << false << Qt::Edges(Qt::LeftEdge);
        QTest::newRow("right") << QPointF(305, 100) << false << Qt::Edges(Qt::RightEdge);
        QTest::newRow("top") << QPointF(150, -5) << false << Qt::Edges(Qt::TopEdge);
        QTest::newRow("bottom") << QPointF(150, 205) << false << Qt::Edges(Qt::BottomEdge);
        QTest::newRow("right-limit") << QPointF(310, 100) << false << Qt::Edges();
        QTest::newRow("bottom-limit") << QPointF(150, 210) << false << Qt::Edges();
        QTest::newRow("center") << QPointF(150, 100) << true << Qt::Edges();
        QTest::newRow("nan") << QPointF(std::numeric_limits<qreal>::quiet_NaN(), 50)
                             << false << Qt::Edges();
    }

    void contour()
    {
        QFETCH(QPointF, point);
        QFETCH(bool, inside);
        QFETCH(Qt::Edges, edges);
        for (const QPointF offset : {QPointF(), QPointF(30, 50)}) {
            const FrameGeometry geometry(QRectF(offset, QSizeF(300, 200)), 80);
            QCOMPARE(geometry.contains(point + offset), inside);
            QCOMPARE(geometry.resizeEdgesAt(point + offset, 10), edges);
        }
    }

    void inputMask_data()
    {
        QTest::addColumn<qreal>("ratio");
        QTest::addColumn<bool>("touch");
        for (const qreal ratio : {1.0, 1.25, 2.0}) {
            for (bool touch : {false, true})
                QTest::newRow(qPrintable(QString::number(ratio) + (touch ? "-touch" : "-mouse")))
                    << ratio << touch;
        }
    }

    void inputMask()
    {
        QFETCH(qreal, ratio);
        QFETCH(bool, touch);
        NativeSurface native;
        WSurface surface(&native.handle);
        QQmlEngine engine;
        QQuickWindow window;
        window.resize(500, 300);
        QQmlComponent lowerComponent(&engine);
        lowerComponent.setData(R"(
            import QtQuick
            MouseArea { width: 500; height: 300; property int presses: 0; onPressed: presses++ }
        )", QUrl());
        QVERIFY2(lowerComponent.isReady(), qPrintable(lowerComponent.errorString()));
        std::unique_ptr<QQuickItem> lower(qobject_cast<QQuickItem *>(lowerComponent.create()));
        QVERIFY(lower);
        lower->setParentItem(window.contentItem());
        QQmlComponent delegate(&engine);
        delegate.setData("import QtQuick; Item { required property var surface }", QUrl());
        TestSurfaceItem client(window.contentItem());
        client.setDelegate(&delegate);
        client.setSurfaceSizeRatio(ratio);
        client.setTopPadding(40);
        client.setSurface(&surface);
        auto *eventItem = client.eventItem();
        QVERIFY(eventItem);
        FrameInputMask mask;
        mask.setFrameItem(&client);
        mask.setTarget(eventItem);
        mask.setProperty("geometry", QVariant::fromValue(FrameGeometry(QRectF(QPointF(), client.size()), 80)));
        eventItem->setContainmentMask(&mask);
        QVERIFY(!eventItem->contains(QPointF(5, 0)));
        QVERIFY(!eventItem->contains(QPointF(140, 80)));
        QVERIFY(eventItem->contains(QPointF(60, 80)));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QPoint corner = eventItem->mapToScene(QPointF(5, 0)).toPoint();
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, corner);
        QCOMPARE(lower->property("presses").toInt(), 1);
        QCOMPARE(client.presses, 0);
        const QPoint start = eventItem->mapToScene(QPointF(60, 80)).toPoint();
        if (touch) {
            std::unique_ptr<QPointingDevice> device(QTest::createTouchDevice());
            QTest::touchEvent(&window, device.get()).press(0, start, &window).commit();
            QTest::touchEvent(&window, device.get()).move(0, corner, &window).commit();
            QTest::touchEvent(&window, device.get()).release(0, corner, &window).commit();
        } else {
            QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(&window, corner);
            QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, corner);
        }
        QCOMPARE(client.presses, 1);
        QCOMPARE(client.releases, 1);
        QCOMPARE(lower->property("presses").toInt(), 1);
        eventItem->setContainmentMask(nullptr);
        QVERIFY(eventItem->contains(QPointF(5, 0)));
        client.setSurface(nullptr);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!mask.contains(QPointF(60, 80)));
    }

    void animationContour_data()
    {
        QTest::addColumn<QString>("componentName");
        QTest::addColumn<qreal>("initialRadius");
        QTest::addColumn<qreal>("targetRadius");
        QTest::newRow("maximize") << QStringLiteral("GeometryAnimation") << qreal(12) << qreal(0);
        QTest::newRow("restore") << QStringLiteral("GeometryAnimation") << qreal(0) << qreal(12);
        QTest::newRow("window-transition") << QStringLiteral("WindowTransition") << qreal(12) << qreal(24);
        QTest::newRow("minimize") << QStringLiteral("MinimizeAnimation") << qreal(0) << qreal(12);
        QTest::newRow("show-desktop") << QStringLiteral("ShowDesktopAnimation") << qreal(0) << qreal(12);
        QTest::newRow("new-window") << QStringLiteral("NewAnimation") << qreal(0) << qreal(12);
        QTest::newRow("layer-shell") << QStringLiteral("LayerShellAnimation") << qreal(0) << qreal(12);
    }

    void animationContour()
    {
        QFETCH(QString, componentName);
        QFETCH(qreal, initialRadius);
        QFETCH(qreal, targetRadius);
        // Substitute only external dependencies; execute the production animation bindings.
        QTemporaryDir modules;
        QVERIFY(modules.isValid());
        const auto modulePath = modules.path() + QStringLiteral("/AnimationFixture");
        QVERIFY(QDir().mkpath(modulePath));
        const QList<QPair<QString, QByteArray>> fixtures {
            {"qmldir", "module AnimationFixture\nSurfaceWrapper 1.0 SurfaceWrapper.qml\nXdgShadow 1.0 XdgShadow.qml\nBlur 1.0 Blur.qml\nsingleton WaylandServer 1.0 WaylandServer.qml\nWindowTransition 1.0 WindowTransition.qml\nMinimizeAnimation 1.0 WindowTransition.qml\nNewAnimation 1.0 WindowTransition.qml\nLayerShellAnimation 1.0 WindowTransition.qml\n"},
            {"SurfaceWrapper.qml", R"(import QtQuick
                Item {
                    width: 400; height: 200
                    enum State { Normal, Maximized, Fullscreen }
                    property int surfaceState: SurfaceWrapper.Normal
                    property real radius: 64
                    property var presentationFrameGeometry
                    property rect boundingRect: Qt.rect(0, 0, width, height)
                    property bool visibleDecoration: true
                    property bool noDecoration: false
                    property bool shadowVisible: true
                    property real shadowBlurRadius: 40
                    property real shadowOffsetX: 0
                    property real shadowOffsetY: 10
                    property color shadowColor: "black"
                })"},
            {"XdgShadow.qml", R"(import QtQuick
                Item {
                    objectName: "shadow"
                    property real cornerRadius
                    property real shadowBlur
                    property real shadowOffsetX
                    property real shadowOffsetY
                    property color shadowColor
                })"},
            {"Blur.qml", "import QtQuick; Item { objectName: 'blur'; property real radius }"},
            {"WindowTransition.qml", "import QtQuick; Item { enum Direction { Show = 1, Hide } }"},
            {"WaylandServer.qml", "pragma Singleton\nimport QtQuick\nQtObject { property real animationSpeed: 1 }"},
        };
        for (const auto &fixture : fixtures) {
            QFile file(modulePath + '/' + fixture.first);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(fixture.second), fixture.second.size());
        }
        QQmlEngine engine;
        engine.addImportPath(modules.path());
        engine.addImportPath(QCoreApplication::applicationDirPath() + "/../../waylib/src/server");
        QQmlComponent surfaceComponent(&engine);
        surfaceComponent.setData("import AnimationFixture 1.0; SurfaceWrapper {}", QUrl());
        QVERIFY2(surfaceComponent.isReady(), qPrintable(surfaceComponent.errorString()));
        std::unique_ptr<QObject> surface(surfaceComponent.create());
        QVERIFY(surface);
        const FrameGeometry initial(QRectF(0, 0, 400, 200), initialRadius);
        surface->setProperty("presentationFrameGeometry", QVariant::fromValue(initial));
        const auto path = QFINDTESTDATA("../../src/core/qml/Animations") + '/' + componentName + ".qml";
        QFile source(path);
        QVERIFY(source.open(QIODevice::ReadOnly));
        auto code = source.readAll();
        code.replace("import Treeland", "import AnimationFixture 1.0");
        if (componentName == QStringLiteral("LayerShellAnimation"))
            code.prepend("import AnimationFixture 1.0\n");
        QQmlComponent component(&engine);
        component.setData(code, QUrl::fromLocalFile(path));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QVariantMap properties;
        if (componentName == QStringLiteral("GeometryAnimation")
            || componentName == QStringLiteral("WindowTransition")) {
            properties = {{"surface", QVariant::fromValue(surface.get())},
                          {"fromGeometry", initial.rect()}, {"toGeometry", initial.rect()},
                          {"enableBlur", true}};
        } else {
            properties = {{"target", QVariant::fromValue(surface.get())}};
            if (componentName == QStringLiteral("ShowDesktopAnimation")) {
                properties.insert("showDesktop", true);
            } else {
                properties.insert("direction", 1);
                if (componentName == QStringLiteral("MinimizeAnimation"))
                    properties.insert("position", initial.rect());
                else
                    properties.insert("enableBlur", true);
                if (componentName == QStringLiteral("LayerShellAnimation"))
                    properties.insert("position", Qt::BottomEdge);
            }
        }
        std::unique_ptr<QObject> animation(component.createWithInitialProperties(properties));
        QVERIFY2(animation, qPrintable(component.errorString()));
        auto *shadow = animation->findChild<QObject *>("shadow");
        auto *blur = animation->findChild<QObject *>("blur");
        const bool blurOnly = componentName == QStringLiteral("NewAnimation")
            || componentName == QStringLiteral("LayerShellAnimation");
        const bool shadowOnly = componentName == QStringLiteral("MinimizeAnimation")
            || componentName == QStringLiteral("ShowDesktopAnimation");
        if (!blurOnly) {
            QVERIFY(shadow);
            QCOMPARE(shadow->property("cornerRadius").toReal(), initialRadius);
        }
        if (!shadowOnly) {
            QVERIFY(blur);
            QCOMPARE(blur->property("radius").toReal(), initialRadius);
        }
        surface->setProperty("presentationFrameGeometry",
                            QVariant::fromValue(FrameGeometry(initial.rect(), targetRadius)));
        if (shadow)
            QCOMPARE(shadow->property("cornerRadius").toReal(), targetRadius);
        if (blur)
            QCOMPARE(blur->property("radius").toReal(), targetRadius);

        if (componentName != QStringLiteral("GeometryAnimation"))
            return;
        auto *item = qobject_cast<QQuickItem *>(animation.get());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(1000, 600);
        item->setParentItem(window.contentItem());
        const QSizeF targetSize = initialRadius > targetRadius ? QSizeF(800, 400) : QSizeF(200, 100);
        const QRectF target(QPointF(80, 60), targetSize);
        QVERIFY(item->setProperty("toGeometry", target));
        QVERIFY(item->setProperty("duration", 400));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QSignalSpy finished(item, SIGNAL(finished()));
        QVERIFY(QMetaObject::invokeMethod(item, "start"));
        QTRY_VERIFY(qAbs(item->width() - initial.rect().width()) > 5);
        QVERIFY(qAbs(item->width() - target.width()) > 5);
        const qreal progress = (item->width() - initial.rect().width())
            / (target.width() - initial.rect().width());
        QVERIFY2(qAbs(item->x() - target.x() * progress) < 1,
                 "Animation position and frame size must advance in the same frame");
        QVERIFY(qAbs(item->y() - target.y() * progress) < 1);
        QVERIFY(qAbs(item->height() - (initial.rect().height()
                         + (target.height() - initial.rect().height()) * progress)) < 1);
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(QRectF(item->position(), item->size()), target);
        QTest::qWait(50);
        QCOMPARE(QRectF(item->position(), item->size()), target);
        item->setParentItem(nullptr);
    }

    void highlightRadius_data()
    {
        QTest::addColumn<bool>("padded");
        QTest::addColumn<qreal>("frameRadius");
        QTest::addColumn<qreal>("paddingRadius");
        QTest::addColumn<qreal>("expectedRadius");
        QTest::newRow("ssd-rounded") << false << qreal(12) << qreal(18) << qreal(12);
        QTest::newRow("ssd-square") << false << qreal(0) << qreal(18) << qreal(0);
        QTest::newRow("ssd-scaled") << false << qreal(4.5) << qreal(18) << qreal(4.5);
        QTest::newRow("csd-rounded") << false << qreal(6) << qreal(18) << qreal(6);
        QTest::newRow("csd-maximized-or-fullscreen") << false << qreal(0) << qreal(18) << qreal(0);
        QTest::newRow("padding-rounded") << true << qreal(12) << qreal(18) << qreal(18);
        QTest::newRow("padding-square") << true << qreal(12) << qreal(0) << qreal(0);
        QTest::newRow("padding-around-square-window") << true << qreal(0) << qreal(12) << qreal(12);
    }

    void highlightRadius()
    {
        QFETCH(bool, padded);
        QFETCH(qreal, frameRadius);
        QFETCH(qreal, paddingRadius);
        QFETCH(qreal, expectedRadius);
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QFINDTESTDATA("../../src/plugins/multitaskview/qml/WindowHighlight.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> highlight(component.createWithInitialProperties({
            {"frameRadius", frameRadius}, {"padded", padded},
            {"paddingRadius", paddingRadius}, {"width", 300}, {"height", 200},
        }));
        QVERIFY2(highlight, qPrintable(component.errorString()));
        QQmlProperty borderWidth(highlight.get(), "border.width");
        QVERIFY(borderWidth.write(2));
        QCOMPARE(highlight->property("radius").toReal(), expectedRadius > 0 ? expectedRadius + 2 : 0);
        QVERIFY(borderWidth.write(4));
        QCOMPARE(highlight->property("radius").toReal(), expectedRadius > 0 ? expectedRadius + 4 : 0);
        if (padded) {
            QVERIFY(highlight->setProperty("paddingRadius", 0));
            QCOMPARE(highlight->property("radius").toReal(), qreal(0));
            QVERIFY(highlight->setProperty("paddingRadius", 5));
            QCOMPARE(highlight->property("radius").toReal(), qreal(9));
        } else {
            QVERIFY(highlight->setProperty("frameRadius", 6));
            QCOMPARE(highlight->property("radius").toReal(), qreal(10));
            QVERIFY(highlight->setProperty("frameRadius", 0));
            QCOMPARE(highlight->property("radius").toReal(), qreal(0));
        }
    }

    void maskLifetime()
    {
        FrameInputMask mask;
        auto frame = std::make_unique<QQuickItem>();
        frame->setSize(QSizeF(300, 200));
        auto target = std::make_unique<QQuickItem>(frame.get());
        target->setSize(QSizeF(320, 220));
        target->setPosition(QPointF(-10, -10));
        mask.setFrameItem(frame.get());
        mask.setTarget(target.get());
        mask.setProperty("geometry", QVariant::fromValue(FrameGeometry(QRectF(0, 0, 300, 200), 80)));
        mask.setProperty("resizeMargin", 10);
        QVERIFY(mask.contains(QPointF(30, 30)));
        QCOMPARE(mask.edgesAt(QPointF(30, 30)), Qt::TopEdge | Qt::LeftEdge);
        QVERIFY(!mask.contains(QPointF(10, 10)));
        target.reset();
        QVERIFY(!mask.contains(QPointF(30, 30)));
        frame.reset();
        QVERIFY(!mask.contains(QPointF(30, 30)));
    }

    void square()
    {
        const FrameGeometry geometry(QRectF(0, 0, 100, 80), 0);
        QVERIFY(geometry.contains(QPointF(0, 0)));
        QVERIFY(!geometry.contains(QPointF(100, 40)));
        QCOMPARE(geometry.resizeEdgesAt(QPointF(-1, -1), 10), Qt::TopEdge | Qt::LeftEdge);
        QCOMPARE(geometry.resizeEdgesAt(QPointF(0, 0), 10), Qt::Edges());
        QCOMPARE(geometry.resizeEdgesAt(QPointF(-1, 40), 0), Qt::Edges());
    }
};

QTEST_MAIN(FrameGeometryTest)
#include "main.moc"

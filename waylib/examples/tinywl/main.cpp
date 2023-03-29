/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <WServer>
#include <WOutput>
#include <WBackend>
#include <WXCursorManager>
#include <WCursor>
#include <WSeat>
#include <WOutputLayout>
#include <WInputDevice>
#include <WXdgShell>
#include <WSurfaceManager>
#include <WSurfaceLayout>
#include <WSurface>
#include <WSurfaceItem>

#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlContext>
#include <QTimer>
#include <QThread>
#include <QQuickView>
#include <QGuiApplication>
#include <QQuickRenderControl>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QApplication>

WAYLIB_SERVER_USE_NAMESPACE

class SurfaceManager : public WSurfaceManager
{
public:
    SurfaceManager(WServer *server, WOutputLayout *layout)
        : WSurfaceManager(server, layout, server)
    {

    }

    QQuickWindow *createWindow(QQuickRenderControl *rc, WOutput *output) override {
        ensureEngine();
        qmlRegisterUncreatableType<WOutput>("org.zjide.waylib", 1, 0, "Output", "Don't crate");
        auto view = new QQuickView(QUrl(), rc);
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->setInitialProperties({{"output", QVariant::fromValue(output)}});
        view->setSource(QUrl("qrc:/Output.qml"));
        connect(view->engine(), &QQmlEngine::exit, qApp, &QGuiApplication::exit);
        connect(view->engine(), &QQmlEngine::quit, qApp, &QGuiApplication::quit);
        return view;
    }

    QQuickItem *createSurfaceItem(WSurface *surface) override {
        if (!surface->testAttribute(WSurface::Attribute::Immovable)) {
            // init the position for the surface
            if (auto output = surface->attachedOutput()) {
                QRectF surface_geometry(QPointF(0, 0), surface->size());
                const QRectF output_geometry(output->position(), output->effectiveSize());
                surface_geometry.moveCenter(output_geometry.center());
                setPosition(surface, surface_geometry.topLeft());
            }
        }

        if (!surfaceDelegate) {
            ensureEngine();
            qmlRegisterType<WSurfaceItem>("org.zjide.waylib", 1, 0, "SurfaceItem");
            surfaceDelegate = new QQmlComponent(engine, this);
            surfaceDelegate->loadUrl(QUrl("qrc:/SurfaceDelegate.qml"));
        }

        const QVariantMap properies {
            {"surface", QVariant::fromValue(surface)}
        };
        auto item = qobject_cast<QQuickItem*>(surfaceDelegate->createWithInitialProperties(properies));
        auto surfaceItem = item->findChild<WSurfaceItem*>();
        Q_ASSERT(surfaceItem);
        qRegisterMetaType<Qt::ApplicationState>();
        // Active the window on clicked
        connect(surfaceItem, &WSurfaceItem::mouseRelease, this, [surfaceItem, this] (WSeat *seat) {
            requestActivate(surfaceItem->surface(), seat);
        });
        return item;
    }

    QQuickItem *layerItem(QQuickWindow *window, Layer layer) const override {
        if (layer == Layer::Window) {
            return window->contentItem()->findChild<QQuickItem*>("WindowLayer");
        }

        return WSurfaceManager::layerItem(window, layer);
    }

private:
    void ensureEngine() {
        if (!engine) {
            engine = new QQmlEngine(this);
            engine->rootContext()->setContextProperty("manager", this);
        }
    }

    QQmlEngine *engine = nullptr;
    QQmlComponent *surfaceDelegate = nullptr;
};

int main(int argc, char *argv[]) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    QScopedPointer<WServer> server(new WServer());
    WOutputLayout *layout = new WOutputLayout;
    SurfaceManager *manager = new SurfaceManager(server.get(), layout);

    server->attach<WBackend>(layout);
    auto seat = server->attach<WSeat>();
    server->attach<WXdgShell>(manager);

    WXCursorManager xcursor_manager;
    WCursor cursor;

    cursor.setManager(&xcursor_manager);
    cursor.setOutputLayout(layout);
    seat->attachCursor(&cursor);

    QObject::connect(server.get(), &WServer::inputAdded,
                     [&] (WBackend*, WInputDevice *device) {
        seat->attachInputDevice(device);
    });

    server->start();
    {
        bool ok = server->waitForStarted();
        Q_ASSERT(ok);
    }

#if QT_VERSION_MAJOR < 6
    // Because the wlr_egl only have the color buffer
    qputenv("QSG_NO_DEPTH_BUFFER", "1");
    qputenv("QSG_NO_STENCIL_BUFFER", "1");
#else
    QQuickStyle::setStyle("Material");
#endif

    qputenv("QT_QPA_PLATFORM", "wayland");
    qputenv("WAYLAND_DISPLAY", server->displayName());


    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
//    QGuiApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
//    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QApplication app(argc, argv);

    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     server.get(), &WServer::stop, Qt::DirectConnection);

    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/Window.qml"));

    manager->initialize();

    return app.exec();
}

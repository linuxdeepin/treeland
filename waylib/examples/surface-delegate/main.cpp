// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WOutput>


#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QProcess>
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/x11.h>
#include <wlr/backend/wayland.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/util/log.h>

WAYLIB_SERVER_USE_NAMESPACE

class Q_DECL_HIDDEN Helper : public QObject
{
    Q_OBJECT
public:
    explicit Helper(QObject *parent = nullptr)
        : QObject(parent) {}

    Q_INVOKABLE void startDemo(const QString &socket) {
        QProcess waylandClientDemo;

        waylandClientDemo.setProgram("gedit");
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WAYLAND_DISPLAY", socket);

        waylandClientDemo.setProcessEnvironment(env);
        waylandClientDemo.startDetached();
    }
};

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_INFO, nullptr);
    WServer::initializeQPA();
//    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine waylandEngine;
    waylandEngine.rootContext()->setContextProperty("helper", QVariant::fromValue(new Helper()));
    waylandEngine.loadFromModule("SurfaceDelegate", "Main");

    WServer *server = waylandEngine.rootObjects().first()->findChild<WServer*>();
    Q_ASSERT(server);
    Q_ASSERT(server->isRunning());

    return app.exec();
}

#include "main.moc"

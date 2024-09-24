// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WOutput>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwlogging.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QProcess>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

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
    qw_log::init();
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

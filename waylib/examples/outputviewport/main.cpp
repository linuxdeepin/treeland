// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>

#include <qwlogging.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

int main(int argc, char *argv[]) {
    QWLog::init();
    WServer::initializeQPA();
//    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine waylandEngine;
    waylandEngine.loadFromModule("Examples.OutputViewport", "Main");

    WServer *server = waylandEngine.rootObjects().first()->findChild<WServer*>();
    Q_ASSERT(server);
    Q_ASSERT(server->isRunning());

    return app.exec();
}

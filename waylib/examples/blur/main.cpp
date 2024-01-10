// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WOutput>
// TODO: Don't use private API
#include <wquickbackend_p.h>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

int main(int argc, char *argv[]) {
    WServer::initializeQPA();
//    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine waylandEngine;
    waylandEngine.loadFromModule("Blur", "Main");

    WServer *server = waylandEngine.rootObjects().first()->findChild<WServer*>();
    Q_ASSERT(server);
    Q_ASSERT(server->isRunning());

    auto backend = server->findChild<WQuickBackend*>();
    Q_ASSERT(backend);

    // multi output
   qobject_cast<QWMultiBackend*>(backend->backend())->forEachBackend([] (wlr_backend *backend, void *) {
       if (auto x11 = QWX11Backend::from(backend))
           x11->createOutput();
   }, nullptr);

    return app.exec();
}

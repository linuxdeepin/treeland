// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <wrenderhelper.h>
#include <qwbuffer.h>
#include <qwlogging.h>

#include <QGuiApplication>

WAYLIB_SERVER_USE_NAMESPACE

int main(int argc, char *argv[]) {
    qw_log::init();

    WRenderHelper::setupRendererBackend();
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    WServer::initializeQPA();
    //    QQuickStyle::setStyle("Material");

    QPointer<Helper> helper;
    int quitCode = 0;
    {
        QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        QGuiApplication::setQuitOnLastWindowClosed(false);
        QGuiApplication app(argc, argv);

        QmlEngine *qmlEngine = new QmlEngine;

        QObject::connect(qmlEngine, &QQmlEngine::quit, qmlEngine, &QmlEngine::deleteLater);
        QObject::connect(qmlEngine, &QQmlEngine::exit, &app, [qmlEngine, &quitCode] (int code) {
            quitCode = code;
            qmlEngine->deleteLater();
        });
        QObject::connect(qmlEngine, &QmlEngine::destroyed, &app, [&] {
            // make sure all deleted before app exit
            app.exit(quitCode);
        });

        Helper *helper = qmlEngine->singletonInstance<Helper*>("Tinywl", "Helper");
        QObject::connect(helper, &Helper::requestQuit, qmlEngine, &QmlEngine::deleteLater);
        helper->init();

        quitCode = app.exec();
    }

    Q_ASSERT(!helper);
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    return quitCode;
}

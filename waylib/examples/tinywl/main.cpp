// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wlogging.h>
#include "helper.h"

#include <wrenderhelper.h>

#include <wlr_all.h>

#include <QGuiApplication>

WAYLIB_SERVER_USE_NAMESPACE

int main(int argc, char *argv[]) {
    WLog::init();

    WRenderHelper::setupRendererBackend();
    Q_ASSERT(waylib_buffer_get_count() == 0);

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
    Q_ASSERT(waylib_buffer_get_count() == 0);

    return quitCode;
}

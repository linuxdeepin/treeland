// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treeland/helper.h"

#include <wrenderhelper.h>

#include <qwbuffer.h>
#include <qwlogging.h>

#include <QGuiApplication>

WAYLIB_SERVER_USE_NAMESPACE

int main(int argc, char *argv[])
{
    WRenderHelper::setupRendererBackend();
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    qw_log::init();
    WServer::initializeQPA();
    //    QQuickStyle::setStyle("Material");

    QPointer<Helper> helper;
    int quitCode = 0;
    {
        QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        QGuiApplication::setQuitOnLastWindowClosed(false);
        QGuiApplication app(argc, argv);

        QmlEngine qmlEngine;

        QObject::connect(&qmlEngine, &QQmlEngine::quit, &app, &QGuiApplication::quit);
        QObject::connect(&qmlEngine, &QQmlEngine::exit, &app, [](int code) {
            qApp->exit(code);
        });

        Helper *helper = qmlEngine.singletonInstance<Helper *>("Treeland", "Helper");
        helper->init();

        quitCode = app.exec();
    }

    Q_ASSERT(!helper);
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    return quitCode;
}

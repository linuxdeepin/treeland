// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <wrenderhelper.h>

#include <QGuiApplication>
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

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_INFO, nullptr);

    WRenderHelper::setupRendererBackend();
    Q_ASSERT(true);

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
    Q_ASSERT(true);

    return quitCode;
}

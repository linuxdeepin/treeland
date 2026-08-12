// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treelandinit.h"
#include <DGuiApplicationHelper>
#include <QGuiApplication>
#include <wrenderhelper.h>
#include <wbackend.h>
#include <wserver.h>

#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE
DCORE_USE_NAMESPACE

namespace Treeland {

void preInit(const InitOptions &opts)
{
    if (opts.headless) {
        const auto testBackends = qgetenv("TREELAND_TEST_WLR_BACKENDS");
        qputenv("WLR_BACKENDS", testBackends.isEmpty() ? "headless" : testBackends);
    }
    DTK_GUI_NAMESPACE::DGuiApplicationHelper::setAttribute(
        DTK_GUI_NAMESPACE::DGuiApplicationHelper::DontSaveApplicationTheme, true);
    WServer::initializeQPA({}, opts.createPlatformTheme);
    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
}

void initTestServer(WServer *server)
{
    auto *backend = server->attach<WBackend>();
    auto *renderer = WRenderHelper::createRenderer(backend->handle());
    wlr_compositor_create(server->handle(), 6, renderer);
    wlr_subcompositor_create(server->handle());
}

void postInit()
{
    WRenderHelper::setupRendererBackend();
}

}

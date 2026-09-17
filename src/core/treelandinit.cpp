// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treelandinit.h"

#include "deepintheme.h"

#include <DGuiApplicationHelper>
#include <QGuiApplication>
#include <QQuickStyle>
#include <wrenderhelper.h>
#include <wlogging.h>
#include <wbackend.h>
#include <wserver.h>

#include <qpa/qplatformtheme.h>

#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE
DCORE_USE_NAMESPACE

namespace Treeland {

static QDeepinTheme *g_theme = nullptr;

QDeepinTheme *deepinTheme()
{
    return g_theme;
}

std::unique_ptr<QGuiApplication> preInit(int &argc, char *argv[])
{
    WLog::init();
    DTK_GUI_NAMESPACE::DGuiApplicationHelper::setAttribute(
        DTK_GUI_NAMESPACE::DGuiApplicationHelper::DontSaveApplicationTheme, true);
    WServer::initializeQPA({}, [](const QString &) {
        g_theme = new QDeepinTheme();
        return static_cast<QPlatformTheme *>(g_theme);
    });
    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle("Chameleon");

    auto application = std::make_unique<QGuiApplication>(argc, argv);
    application->setOrganizationName("deepin");
    application->setApplicationName("treeland");
    return application;
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

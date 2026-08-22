// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treelandinit.h"

#include <DGuiApplicationHelper>
#include <QGuiApplication>
#include <QPalette>
#include <QQuickStyle>
#include <wrenderhelper.h>
#include <wlogging.h>
#include <wbackend.h>
#include <wserver.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#  include <private/qgenericunixtheme_p.h>
#else
#  include <private/qgenericunixthemes_p.h>
#endif

#include <qpa/qplatformtheme.h>

#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE
DCORE_USE_NAMESPACE

namespace Treeland {

namespace {
class QDeepinTheme : public QGenericUnixTheme
{
public:
    const QPalette *palette(QPlatformTheme::Palette type) const override
    {
        if (type != QPlatformTheme::SystemPalette)
            return QGenericUnixTheme::palette(type);
        static QPalette palette;
        palette = Dtk::Gui::DGuiApplicationHelper::instance()->applicationPalette();
        return &palette;
    }
};
}

std::unique_ptr<QGuiApplication> preInit(int &argc, char *argv[])
{
    WLog::init();
    DTK_GUI_NAMESPACE::DGuiApplicationHelper::setAttribute(
        DTK_GUI_NAMESPACE::DGuiApplicationHelper::DontSaveApplicationTheme, true);
    WServer::initializeQPA({}, [](const QString &) {
        return static_cast<QPlatformTheme *>(new QDeepinTheme());
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

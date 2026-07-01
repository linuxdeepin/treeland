// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/treeland.h"
#include "deepintheme.h"
#include "input/inputmanager.h"
#include "seat/helper.h"
#include "utils/cmdline.h"

#include <wrenderhelper.h>

#include <qwbuffer.h>
#include <qwlogging.h>

#include <DGuiApplicationHelper>
#include <DLog>

#include <QGuiApplication>
#include <QMetaType>

#include <wserver.h>

#include <qpa/qplatformtheme.h>

WAYLIB_SERVER_USE_NAMESPACE
DCORE_USE_NAMESPACE;

static QDeepinTheme *g_theme = nullptr;

static void bindThemeConfig()
{
    auto *helper = Helper::instance();
    if (!helper || !g_theme)
        return;

    g_theme->bindConfig(helper->config());
}

int main(int argc, char *argv[])
{
    qw_log::init();
    DTK_GUI_NAMESPACE::DGuiApplicationHelper::setAttribute(
        DTK_GUI_NAMESPACE::DGuiApplicationHelper::DontSaveApplicationTheme,
        true);
    WServer::initializeQPA({}, [](const QString &) {
        g_theme = new QDeepinTheme();
        return static_cast<QPlatformTheme *>(g_theme);
    });

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QGuiApplication app(argc, argv);

    app.setOrganizationName("deepin");
    app.setApplicationName("treeland");

#ifdef QT_DEBUG
    DLogManager::registerConsoleAppender();
#endif

    // Enable console logging in non-debug builds via --console-log flag
    CmdLine::ref();
#ifndef QT_DEBUG
    if (CmdLine::ref().consoleLog()) {
        DLogManager::registerConsoleAppender();
    }
#endif
    DLogManager::registerJournalAppender();

    WRenderHelper::setupRendererBackend();
    if (CmdLine::ref().tryExec())
        return 0;
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    int quitCode = 0;
    {
        Treeland::Treeland treeland;

        bindThemeConfig();
        QObject::connect(Helper::instance(), &Helper::configChanged, &bindThemeConfig);
        QObject::connect(Helper::instance()->inputManager(), &InputManager::seatConfigChanged,
                         g_theme, &QDeepinTheme::bindSeatConfig);

        quitCode = app.exec();
    }

    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    return quitCode;
}

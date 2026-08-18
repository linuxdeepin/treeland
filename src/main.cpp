// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wlogging.h>
#include "core/treeland.h"
#include "common/treelandlogging.h"
#include "utils/cmdline.h"

#include <wrenderhelper.h>

#include <wlr_all.h>

#include <DGuiApplicationHelper>
#include <DLog>

#include <QByteArray>
#include <QGuiApplication>
#include <QMetaType>
#include <QPalette>
#include <QQuickWindow>
#include <QQuickStyle>

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#  include <private/qgenericunixtheme_p.h>
#else
#  include <private/qgenericunixthemes_p.h>
#endif

#include <wserver.h>

#include <qpa/qplatformtheme.h>

WAYLIB_SERVER_USE_NAMESPACE
DCORE_USE_NAMESPACE;

class QDeepinTheme : public QGenericUnixTheme
{
public:
    const QPalette *palette(QPlatformTheme::Palette type) const override
    {
        if (type != QPlatformTheme::SystemPalette) {
            return QGenericUnixTheme::palette(type);
        }
        static QPalette palette;
        palette = Dtk::Gui::DGuiApplicationHelper::instance()->applicationPalette();
        return &palette;
    }
};

int main(int argc, char *argv[])
{
    WLog::init();
    const QByteArray wlrRenderer = qgetenv("WLR_RENDERER");
    const bool explicitVulkanRenderer = wlrRenderer == "vulkan";
    DTK_GUI_NAMESPACE::DGuiApplicationHelper::setAttribute(
        DTK_GUI_NAMESPACE::DGuiApplicationHelper::DontSaveApplicationTheme,
        true);
    WServer::initializeQPA({}, [](const QString &) {
        return static_cast<QPlatformTheme *>(new QDeepinTheme());
    });
    QQuickStyle::setStyle("Chameleon");

    if (explicitVulkanRenderer)
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    else
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
    if (explicitVulkanRenderer) {
        qCInfo(lcTlCore) << "Explicit Vulkan renderer requested; Qt::AA_UseOpenGLES was not set";
    }

    WRenderHelper::setupRendererBackend();
    if (CmdLine::ref().tryExec())
        return 0;
    Q_ASSERT(waylib_buffer_get_count() == 0);

    int quitCode = 0;
    {
        Treeland::Treeland treeland;

        quitCode = app.exec();
    }
    Q_ASSERT(waylib_buffer_get_count() == 0);

    return quitCode;
}

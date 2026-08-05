// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/treeland.h"
#include "core/systemdconfigmanager.h"
#include "common/treelandlogging.h"
#include "utils/cmdline.h"

#include <wrenderhelper.h>

#include <qwbuffer.h>
#include <qwlogging.h>

#include <DGuiApplicationHelper>
#include <DLog>

#include <QGuiApplication>
#include <QMetaType>
#include <QPalette>

#include <memory>

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
    qw_log::init();
    DTK_GUI_NAMESPACE::DGuiApplicationHelper::setAttribute(
        DTK_GUI_NAMESPACE::DGuiApplicationHelper::DontSaveApplicationTheme,
        true);
    WServer::initializeQPA({}, [](const QString &) {
        return static_cast<QPlatformTheme *>(new QDeepinTheme());
    });
    //    QQuickStyle::setStyle("Material");

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
        SystemDConfigManager systemDConfigManager(&app);
        std::unique_ptr<Treeland::Treeland> treeland;

        auto startTreeland = [&treeland] {
            if (!treeland) {
                treeland = std::make_unique<Treeland::Treeland>();
            }
        };

        QObject::connect(&systemDConfigManager,
                         &SystemDConfigManager::InitializeSucceed,
                         &app,
                         startTreeland);
        QObject::connect(&systemDConfigManager,
                         &SystemDConfigManager::InitializeFailed,
                         &app,
                         [&app] {
                             qCCritical(lcTlCore)
                                 << "Global DConfig initialization failed; aborting Treeland startup.";
                             app.exit(1);
                         });

        if (systemDConfigManager.isInitializeFailed()) {
            qCCritical(lcTlCore)
                << "Global DConfig initialization failed before the event loop started.";
            quitCode = 1;
        } else {
            if (systemDConfigManager.isInitializeSucceeded()) {
                startTreeland();
            }

            quitCode = app.exec();
        }
    }

    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    return quitCode;
}

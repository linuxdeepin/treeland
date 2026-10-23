// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "common/treelandlogging.h"
#include "dconfig/dconfigmanager.h"
#include "core/treeland.h"
#include "core/treelandinit.h"
#include "core/waylandserver.h"
#include "deepintheme.h"
#include "input/inputmanager.h"
#include "utils/cmdline.h"

#include <wrenderhelper.h>

#include <wlr_all.h>

#include <DLog>
#include <QGuiApplication>

#include <memory>

static void bindThemeConfig()
{
    auto *server = WaylandServer::instance();
    auto *theme = Treeland::deepinTheme();
    if (!server || !theme)
        return;

    theme->bindConfig(server->config());
}

int main(int argc, char *argv[])
{
    auto application = Treeland::preInit(argc, argv);

#ifdef QT_DEBUG
    DLogManager::registerConsoleAppender();
#endif
    CmdLine::ref();
#ifndef QT_DEBUG
    if (CmdLine::ref().consoleLog())
        DLogManager::registerConsoleAppender();
#endif
    DLogManager::registerJournalAppender();

    Treeland::postInit();

    if (CmdLine::ref().tryExec())
        return 0;
    Q_ASSERT(waylib_buffer_get_count() == 0);

    int quitCode = 0;
    {
        DConfigManager dConfigManager(application.get());
        std::unique_ptr<Treeland::Treeland> treeland;

        auto startTreeland = [&treeland] {
            if (!treeland) {
                qCInfo(lcTlConfig) << "DConfig initialization completed; starting Treeland";
                treeland = std::make_unique<Treeland::Treeland>();

                auto *server = WaylandServer::instance();
                bindThemeConfig();
                QObject::connect(server, &WaylandServer::configChanged, server, &bindThemeConfig);
                QObject::connect(server->inputManager(),
                                 &InputManager::seatConfigChanged,
                                 server,
                                 [](SeatUserDConfig *config) {
                                     Treeland::deepinTheme()->bindSeatConfig(config);
                                 });
            }
        };

        QObject::connect(&dConfigManager,
                         &DConfigManager::initializeSucceed,
                         application.get(),
                         startTreeland);
        QObject::connect(&dConfigManager,
                         &DConfigManager::initializeFailed,
                         application.get(),
                         [application = application.get()] {
                             qCCritical(lcTlCore)
                                 << "Global DConfig initialization failed; aborting Treeland startup.";
                             application->exit(1);
                         });

        if (dConfigManager.isInitializeFailed()) {
            qCCritical(lcTlCore)
                << "Global DConfig initialization failed before the event loop started.";
            quitCode = 1;
        } else {
            if (dConfigManager.isInitializeSucceeded()) {
                startTreeland();
            } else {
                qCInfo(lcTlConfig) << "Waiting for global DConfig initialization before starting Treeland";
            }

            quitCode = application->exec();
        }
    }
    Q_ASSERT(waylib_buffer_get_count() == 0);

    return quitCode;
}

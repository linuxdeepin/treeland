// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/treeland.h"
#include "core/treelandinit.h"
#include "deepintheme.h"
#include "input/inputmanager.h"
#include "seat/helper.h"
#include "utils/cmdline.h"

#include <wrenderhelper.h>

#include <wlr_all.h>

#include <DLog>
#include <QGuiApplication>

static void bindThemeConfig()
{
    auto *helper = Helper::instance();
    auto *theme = Treeland::deepinTheme();
    if (!helper || !theme)
        return;

    theme->bindConfig(helper->config());
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
        Treeland::Treeland treeland;

        bindThemeConfig();
        QObject::connect(Helper::instance(), &Helper::configChanged, &bindThemeConfig);
        QObject::connect(Helper::instance()->inputManager(), &InputManager::seatConfigChanged,
                         [](SeatUserDConfig *config) { Treeland::deepinTheme()->bindSeatConfig(config); });

        quitCode = QGuiApplication::exec();
    }
    Q_ASSERT(waylib_buffer_get_count() == 0);

    return quitCode;
}

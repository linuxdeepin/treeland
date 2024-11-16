// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "cmdline.h"
#include "treeland.h"

#include <wrenderhelper.h>

#include <qwbuffer.h>
#include <qwlogging.h>

#include <DLog>

#include <QGuiApplication>

WAYLIB_SERVER_USE_NAMESPACE
DCORE_USE_NAMESPACE;

int main(int argc, char *argv[])
{
#ifdef QT_DEBUG
    DLogManager::registerConsoleAppender();
#endif
    DLogManager::registerJournalAppender();

    qw_log::init();
    WServer::initializeQPA();
    //    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QGuiApplication app(argc, argv);
    app.setOrganizationName("deepin");
    app.setApplicationName("treeland");

    CmdLine::ref();

    WRenderHelper::setupRendererBackend();
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    Treeland::Treeland treeland;

    int quitCode = app.exec();

    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    return quitCode;
}

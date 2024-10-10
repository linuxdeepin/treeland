// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"
#include "treeland/treeland.h"

#include <wrenderhelper.h>

#include <qwbuffer.h>
#include <qwlogging.h>

#include <DLog>

#include <QGuiApplication>

WAYLIB_SERVER_USE_NAMESPACE
DCORE_USE_NAMESPACE;

int main(int argc, char *argv[])
{
    WRenderHelper::setupRendererBackend();
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

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

#ifdef QT_DEBUG
    DLogManager::registerConsoleAppender();
#endif
    DLogManager::registerJournalAppender();

    QCommandLineOption socket({ "s", "socket" }, "set ddm socket", "socket");
    QCommandLineOption run({ "r", "run" }, "run a process", "run");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({ socket, run });

    parser.process(app);

    qmlRegisterModule("Treeland.Greeter", 1, 0);
    qmlRegisterModule("Treeland.Protocols", 1, 0);

    QmlEngine qmlEngine;

    QObject::connect(&qmlEngine, &QQmlEngine::quit, &app, &QGuiApplication::quit);
    QObject::connect(&qmlEngine, &QQmlEngine::exit, &app, [](int code) {
        qApp->exit(code);
    });

    Helper *helper = qmlEngine.singletonInstance<Helper *>("Treeland", "Helper");
    helper->init();

    TreeLand::TreeLand treeland(helper, { parser.value(socket), parser.value(run) });

    int quitCode = app.exec();

    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    return quitCode;
}

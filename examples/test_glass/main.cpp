// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <WServer>
#include <WBackend>
#include <WOutput>
#include <woutputrenderwindow.h>

#include <qwbackend.h>
#include <qwlogging.h>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

QW_USE_NAMESPACE

int main(int argc, char *argv[]) {
    qw_log::init();
    WServer::initializeQPA();
//    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Waylib blur and glass effect demo"));
    parser.addHelpOption();
    const QCommandLineOption wallpaperOption(
        { QStringLiteral("w"), QStringLiteral("wallpaper") },
        QStringLiteral("Wallpaper image path or URL."),
        QStringLiteral("path"));
    parser.addOption(wallpaperOption);
    parser.process(app);

    QString wallpaperSource =
        QStringLiteral("qrc:/qt/qml/GlassExample/assets/default-glass-background.jpg");
    const QString wallpaperOverride = parser.value(wallpaperOption);
    if (!wallpaperOverride.isEmpty()) {
        wallpaperSource = QUrl::fromUserInput(
            wallpaperOverride, QDir::currentPath(), QUrl::AssumeLocalFile).toString();
    }

    QQmlApplicationEngine waylandEngine;
    waylandEngine.loadFromModule("GlassExample", "Main");

    Helper *helper = waylandEngine.singletonInstance<Helper*>("Treeland", "Helper");
    Q_ASSERT(helper);
    helper->setWallpaperSource(wallpaperSource);

    auto window = waylandEngine.rootObjects().first()->findChild<WOutputRenderWindow*>();
    Q_ASSERT(window);

    helper->initProtocols(window, &waylandEngine);

    // multi output
    qobject_cast<qw_multi_backend*>(helper->backend()->handle())->for_each_backend([] (wlr_backend *backend, void *) {
        if (auto x11 = qw_x11_backend::from(backend)) {
            x11->output_create();
        }
    }, nullptr);

    return app.exec();
}

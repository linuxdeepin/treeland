// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelandwallpapernotifierclient.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGRendererInterface>

int main(int argc, char *argv[])
{
    // The entire rendering framework, including mpv, is based on OpenGL.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);

    app.setOrganizationName("deepin");
    app.setApplicationName("treeland-wallpaper-factory");

    std::unique_ptr<TreelandWallpaperNotifierClientV1> produce;
    produce.reset(new TreelandWallpaperNotifierClientV1);
    produce->instantiate();

    return app.exec();
}

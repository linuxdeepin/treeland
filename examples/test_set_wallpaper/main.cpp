// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelandwallpapermanagerclient.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <qpa/qplatformnativeinterface.h>

static QScreen *findScreenByName(const QString &name)
{
    if (name.isEmpty())
        return nullptr;

    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() == name)
            return screen;
    }

    return nullptr;
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "wayland");

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Treeland Wallpaper Client"));
    parser.addHelpOption();

    QCommandLineOption outputNameOption(
        QStringLiteral("output"),
        QStringLiteral("Wayland output name (e.g. eDP-1, HDMI-1). Same as wlr-randr output name."),
        QStringLiteral("output-name")
        );
    parser.addOption(outputNameOption);
    parser.process(app);

    const QString outputName = parser.value(outputNameOption);
    QScreen *targetScreen = findScreenByName(outputName);

    if (!targetScreen) {
        qCritical().noquote()
        << "Cannot find QScreen for output:" << outputName
        << "\nAvailable outputs are:";
        for (QScreen *screen : QGuiApplication::screens())
            qCritical() << " -" << screen->name();
        return EXIT_FAILURE;
    }

    TreelandWallpaperManagerV1 *wallpaperManager = new TreelandWallpaperManagerV1;
    QObject::connect(wallpaperManager, &TreelandWallpaperManagerV1::activeChanged, [wallpaperManager, targetScreen] {
        if (wallpaperManager->isActive()) {
            if (!targetScreen) {
                qWarning() << "createWallpaper called with null QScreen";
                qApp->exit(EXIT_FAILURE);
            }

            auto *nativeInterface = qGuiApp->platformNativeInterface();
            if (!nativeInterface) {
                qCritical() << "No QPlatformNativeInterface available";
                qApp->exit(EXIT_FAILURE);
            }

            wl_output *wlOutput = static_cast<wl_output *>(
                nativeInterface->nativeResourceForScreen(
                    QByteArrayLiteral("output"), targetScreen));

            if (!wlOutput) {
                qCritical() << "Failed to get wl_output for screen:" << targetScreen->name();
                qApp->exit(EXIT_FAILURE);
            }

            auto *object = wallpaperManager->get_treeland_wallpaper(wlOutput, nullptr);
            if (!object) {
                qCritical() << "treeland_wallpaper_manager_v1 returned null object";
                qApp->exit(EXIT_FAILURE);
            }
            TreelandWallpaperV1 *wallpaper = new TreelandWallpaperV1(object);
            wallpaper->set_image_source(
                QStringLiteral("/usr/share/wallpapers/deepin/deepin-default.jpg"),
                QtWayland::treeland_wallpaper_v1::wallpaper_role_desktop
                );
            delete wallpaper;
            delete wallpaperManager;
            qApp->exit(EXIT_SUCCESS);
        }
    });
    wallpaperManager->instantiate();

    return app.exec();
}

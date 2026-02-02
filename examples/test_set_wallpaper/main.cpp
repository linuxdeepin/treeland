// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelandwallpapermanagerclient.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QSet>
#include <QMimeDatabase>
#include <QMimeType>
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

static bool isImageFile(const QString &path)
{
    static QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    return mime.isValid() && mime.name().startsWith("image/");
}

static bool isVideoFile(const QString &path)
{
    static QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    return mime.isValid() && mime.name().startsWith("video/");
}

static uint32_t
parseRole(const QString &role)
{
    using Role = QtWayland::treeland_wallpaper_v1::wallpaper_role;

    if (role == "desktop")
        return Role::wallpaper_role_desktop;
    if (role == "lockscreen")
        return Role::wallpaper_role_lockscreen;
    if (role == "both")
        return Role::wallpaper_role_desktop | Role::wallpaper_role_lockscreen;

    return Role::wallpaper_role_desktop; // default
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
    QCommandLineOption pathOption(
        QStringLiteral("path"),
        QStringLiteral("Wallpaper path (image or video)."),
        QStringLiteral("file-path")
        );
    parser.addOption(pathOption);

    QCommandLineOption roleOption(
        QStringLiteral("role"),
        QStringLiteral("Wallpaper role: desktop | lockscreen | both."),
        QStringLiteral("role"),
        QStringLiteral("desktop")
        );
    parser.addOption(roleOption);
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

    const QString wallpaperPath = parser.value(pathOption);
    const QString roleString = parser.value(roleOption);

    if (wallpaperPath.isEmpty()) {
        qCritical() << "--path is required";
        return EXIT_FAILURE;
    }

    TreelandWallpaperManagerV1 *wallpaperManager = new TreelandWallpaperManagerV1;
    QObject::connect(wallpaperManager, &TreelandWallpaperManagerV1::activeChanged, [&] {
        if (wallpaperManager->isActive()) {
            if (!targetScreen) {
                qWarning() << "createWallpaper called with null QScreen";
                qApp->exit(EXIT_FAILURE);
                return;
            }

            auto *nativeInterface = qGuiApp->platformNativeInterface();
            if (!nativeInterface) {
                qCritical() << "No QPlatformNativeInterface available";
                qApp->exit(EXIT_FAILURE);
                return;
            }

            wl_output *wlOutput = static_cast<wl_output *>(
                nativeInterface->nativeResourceForScreen(
                    QByteArrayLiteral("output"), targetScreen));

            if (!wlOutput) {
                qCritical() << "Failed to get wl_output for screen:" << targetScreen->name();
                qApp->exit(EXIT_FAILURE);
                return;
            }

            auto *object = wallpaperManager->get_treeland_wallpaper(wlOutput, nullptr);
            if (!object) {
                qCritical() << "treeland_wallpaper_manager_v1 returned null object";
                qApp->exit(EXIT_FAILURE);
                return;
            }
            TreelandWallpaperV1 *wallpaper = new TreelandWallpaperV1(object);
            const uint32_t role = parseRole(roleString);
            if (isImageFile(wallpaperPath)) {
                wallpaper->set_image_source(wallpaperPath, role);
            } else if (isVideoFile(wallpaperPath)) {
                wallpaper->set_video_source(wallpaperPath, role);
            } else {
                qCritical() << "Unsupported wallpaper file type:" << wallpaperPath;
                delete wallpaper;
                delete wallpaperManager;
                qApp->exit(EXIT_FAILURE);
                return;
            }
            delete wallpaper;
            delete wallpaperManager;
            qApp->exit(EXIT_SUCCESS);
        }
    });
    wallpaperManager->instantiate();

    return app.exec();
}

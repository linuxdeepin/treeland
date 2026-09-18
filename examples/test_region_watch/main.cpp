// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Test application for treeland_region_watch_manager_v1
// (treeland-region-watch-unstable-v1).
// Monitors an edge-anchored region of the first screen and prints
// enter/leave/output_removed transitions to the console.
//
// Usage: test-region-watch [--anchor top|bottom|left|right] [--width W] [--height H] [--screen NAME]

#include "qwayland-treeland-region-watch-unstable-v1.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QScreen>
#include <QWaylandClientExtension>

#include <qscreen_platform.h>

static QNativeInterface::QWaylandScreen *waylandScreenFor(const QString &name)
{
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (!name.isEmpty() && QString::compare(screen->name(), name, Qt::CaseInsensitive) != 0)
            continue;
        if (auto *waylandScreen = screen->nativeInterface<QNativeInterface::QWaylandScreen>())
            return waylandScreen;
    }
    return nullptr;
}

static QString anchorName(uint32_t anchor)
{
    switch (anchor) {
    case QtWayland::treeland_region_watch_v1::anchor_top:
        return QStringLiteral("top");
    case QtWayland::treeland_region_watch_v1::anchor_bottom:
        return QStringLiteral("bottom");
    case QtWayland::treeland_region_watch_v1::anchor_left:
        return QStringLiteral("left");
    case QtWayland::treeland_region_watch_v1::anchor_right:
        return QStringLiteral("right");
    }
    return QStringLiteral("unknown");
}

class RegionWatchV1 : public QtWayland::treeland_region_watch_v1
{
public:
    explicit RegionWatchV1(struct ::treeland_region_watch_v1 *object)
        : QtWayland::treeland_region_watch_v1(object)
    {
    }

    ~RegionWatchV1() override
    {
        destroy();
    }

protected:
    void treeland_region_watch_v1_enter() override
    {
        qWarning() << "enter: region is overlapped by a toplevel window";
    }

    void treeland_region_watch_v1_leave() override
    {
        qWarning() << "leave: region is clear";
    }

    void treeland_region_watch_v1_output_removed() override
    {
        qWarning() << "output_removed: watcher is inert until the next set_region";
    }
};

class RegionWatchManagerV1
    : public QWaylandClientExtensionTemplate<RegionWatchManagerV1>
    , public QtWayland::treeland_region_watch_manager_v1
{
    Q_OBJECT
public:
    static constexpr int InterfaceVersion = 1;

    explicit RegionWatchManagerV1()
        : QWaylandClientExtensionTemplate<RegionWatchManagerV1>(InterfaceVersion)
    {
    }
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Treeland Region Watch Test Client"));
    parser.addHelpOption();

    QCommandLineOption anchorOption(
        QStringList() << "a" << "anchor",
        QStringLiteral("Edge to anchor the region to: top, bottom, left, right (default: top)."),
        QStringLiteral("edge"),
        QStringLiteral("top"));
    parser.addOption(anchorOption);

    QCommandLineOption widthOption(
        QStringList() << "w" << "width",
        QStringLiteral("Thickness in the anchored direction for left/right (default: 40)."),
        QStringLiteral("pixels"),
        QStringLiteral("40"));
    parser.addOption(widthOption);

    QCommandLineOption heightOption(
        QStringList() << "H" << "height",
        QStringLiteral("Thickness in the anchored direction for top/bottom (default: 40)."),
        QStringLiteral("pixels"),
        QStringLiteral("40"));
    parser.addOption(heightOption);

    QCommandLineOption screenOption(
        QStringList() << "s" << "screen",
        QStringLiteral("Screen name to monitor (default: first screen)."),
        QStringLiteral("name"));
    parser.addOption(screenOption);

    parser.process(app);

    uint32_t anchor = QtWayland::treeland_region_watch_v1::anchor_top;
    const QString anchorValue = parser.value(anchorOption).toLower();
    if (anchorValue == QLatin1String("top")) {
        anchor = QtWayland::treeland_region_watch_v1::anchor_top;
    } else if (anchorValue == QLatin1String("bottom")) {
        anchor = QtWayland::treeland_region_watch_v1::anchor_bottom;
    } else if (anchorValue == QLatin1String("left")) {
        anchor = QtWayland::treeland_region_watch_v1::anchor_left;
    } else if (anchorValue == QLatin1String("right")) {
        anchor = QtWayland::treeland_region_watch_v1::anchor_right;
    } else {
        qCritical() << "Unknown anchor:" << anchorValue
                    << "(expected: top, bottom, left, right)";
        return EXIT_FAILURE;
    }

    bool widthOk = false;
    const int width = parser.value(widthOption).toInt(&widthOk);
    if (!widthOk || width <= 0) {
        qCritical() << "Invalid --width:" << parser.value(widthOption);
        return EXIT_FAILURE;
    }

    bool heightOk = false;
    const int height = parser.value(heightOption).toInt(&heightOk);
    if (!heightOk || height <= 0) {
        qCritical() << "Invalid --height:" << parser.value(heightOption);
        return EXIT_FAILURE;
    }

    auto *manager = new RegionWatchManagerV1();

    QObject::connect(manager, &RegionWatchManagerV1::activeChanged, manager,
                     [manager, anchor, width, height, screen = parser.value(screenOption)]() {
        if (!manager->isActive()) {
            qWarning() << "Protocol not available.";
            return;
        }

        auto *waylandScreen = waylandScreenFor(screen);
        if (!waylandScreen) {
            qCritical() << "Screen not found:" << screen;
            qApp->exit(EXIT_FAILURE);
            return;
        }

        auto *raw = manager->get_region_watch();
        if (!raw) {
            qCritical() << "Failed to create region watch.";
            qApp->exit(EXIT_FAILURE);
            return;
        }

        auto *watch = new RegionWatchV1(raw);
        QObject::connect(qGuiApp, &QCoreApplication::aboutToQuit, qGuiApp, [watch] {
            delete watch;
        });

        watch->set_region(width, height, anchor, waylandScreen->output());

        qWarning() << "Watching region: anchor" << anchorName(anchor)
                << "width" << width << "height" << height
                << "screen" << (screen.isEmpty() ? QStringLiteral("<first>") : screen);
    });

    return app.exec();
}

#include "main.moc"

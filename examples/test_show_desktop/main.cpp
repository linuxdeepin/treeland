// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-show-desktop-unstable-v1.h"

#include <QGuiApplication>
#include <QtWaylandClient/QWaylandClientExtension>

class ShowDesktop
    : public QWaylandClientExtensionTemplate<ShowDesktop>
    , public QtWayland::treeland_show_desktop_v1
{
    Q_OBJECT
public:
    explicit ShowDesktop();

    void treeland_show_desktop_v1_show_desktop_state(uint32_t state)
    {
        qInfo() << "-------Show Desktop State----- " << state;
    }
};

ShowDesktop::ShowDesktop()
    : QWaylandClientExtensionTemplate<ShowDesktop>(1)
{
}

// 显示桌面: ./test-show-desktop 1

// 恢复显示: ./test-show-desktop 0

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);
    ShowDesktop showDesktop;

    QObject::connect(&showDesktop, &ShowDesktop::activeChanged, &showDesktop, [&showDesktop, argc, argv] {
        if (showDesktop.isActive()) {
            if (argc == 2) {
                switch (std::stoi(argv[1])) {
                case showDesktop.state_normal:
                    showDesktop.set_show_desktop_state(showDesktop.state_normal);
                    break;
                case showDesktop.state_show:
                    showDesktop.set_show_desktop_state(showDesktop.state_show);
                    break;
                default:
                    break;
                }
            }
        }
    });

    return app.exec();
}

#include "main.moc"

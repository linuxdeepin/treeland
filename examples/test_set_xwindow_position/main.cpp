// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QtWidgets/QtWidgets>
#include <QtWaylandClient/QWaylandClientExtension>
#include <QTimer>

#include <qpa/qplatformnativeinterface.h>

#include <qwayland-treeland-dde-shell-v1.h>

#include <unistd.h>

class DDEShellManagerV1
    : public QWaylandClientExtensionTemplate<DDEShellManagerV1>
    , public QtWayland::treeland_dde_shell_manager_v1
{
    Q_OBJECT
public:
    DDEShellManagerV1()
        : QWaylandClientExtensionTemplate<DDEShellManagerV1>(
            treeland_dde_shell_manager_v1_interface.version)
    {
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qWarning()
        << "In this example, an xeyes will be launched. Its window will be moved to (0, 0) "
           "first using X API. Then, A test window of size 640x480 will be created. The xeyes "
           "window will be moved to the top-right of the test window once per second using "
           "set_xwindow_position_relative().\n\n"
           "xeyes should be installed to run this example (sudo apt install x11-apps).";

    system("bash -c \"pkill xeyes; xeyes & disown\"");
    sleep(1);
    system("xdotool search --name \"xeyes\" windowmove 0 0");
    sleep(1);
    system("xdotool search --name \"xeyes\" > /tmp/xeyes_wid.txt");

    QWidget *window = new QWidget();
    window->resize(640, 480);
    window->setWindowTitle("Test set xwindow position");
    window->show();

    uint32_t wid = 0;
    QFile file("/tmp/xeyes_wid.txt");
    if (file.open(QIODevice::ReadWrite)) {
        wid = file.readLine().trimmed().toUInt();
        file.close();
        file.remove();
    } else {
        qCritical() << "Failed to open /tmp/xeyes_wid.txt";
        return 1;
    }

    DDEShellManagerV1 manager;

    struct wl_callback_listener callback_listener = { .done = []([[maybe_unused]] void *data,
                                                                 wl_callback *callback,
                                                                 uint32_t ok) {
        wl_callback_destroy(callback);
        if (ok != 0)
            qCritical() << "Failed to set xwindow position relative!";
        else
            qWarning() << "Successfully set xwindow position relative. Check screen for result.";
    } };

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (!manager.isActive()) {
            qCritical() << "DDEShellManagerV1 is not active!";
            return;
        }
        struct wl_surface *surface = static_cast<wl_surface *>(
            QGuiApplication::platformNativeInterface()->nativeResourceForWindow(
                QStringLiteral("surface").toLocal8Bit(),
                window->windowHandle()));

        wl_fixed_t dx = wl_fixed_from_int(640);
        wl_fixed_t dy = wl_fixed_from_int(0);

        wl_callback *callback = manager.set_xwindow_position_relative(wid, surface, dx, dy);
        wl_callback_add_listener(callback, &callback_listener, nullptr);
        qWarning() << "Setting xwindow position relative, wait for result...";
    });
    timer.start(1000);

    return app.exec();
}

#include "main.moc"

// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QtWidgets/QtWidgets>
#include <QtWaylandClient/QWaylandClientExtension>
#include <QTimer>

#include <qpa/qplatformnativeinterface.h>

#include <qwayland-treeland-xwindow-control-unstable-v1.h>

#include <cstdlib>
#include <unistd.h>

class XWindowControlV1
    : public QWaylandClientExtensionTemplate<XWindowControlV1>
    , public QtWayland::treeland_xwindow_control_v1
{
    Q_OBJECT
public:
    XWindowControlV1()
        : QWaylandClientExtensionTemplate<XWindowControlV1>(
            treeland_xwindow_control_v1_interface.version)
    {
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    auto runCommand = [](const char *cmd, const char *desc) {
        const int rc = std::system(cmd);
        if (rc == -1) {
            qCritical() << "Failed to execute command:" << desc;
            return;
        }
        if (rc != 0)
            qWarning() << "Command exited with non-zero status:" << desc << rc;
    };

    qWarning()
        << "In this example, an xeyes will be launched. Its window will be moved to (0, 0) "
           "first using X API. Then, A test window of size 640x480 will be created. The xeyes "
           "window will be moved to the top-right of the test window once per second using "
           "set_xwindow_position_relative().\n\n"
           "xeyes should be installed to run this example (sudo apt install x11-apps).";

    runCommand("bash -c \"pkill xeyes; xeyes & disown\"", "start xeyes");
    sleep(1);
    runCommand("xdotool search --name \"xeyes\" windowmove 0 0", "move xeyes to (0,0)");
    sleep(1);
    runCommand("xdotool search --name \"xeyes\" > /tmp/xeyes_wid.txt", "write xeyes wid");

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

    XWindowControlV1 control;

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
        if (!control.isActive()) {
            qCritical() << "XWindowControlV1 is not active!";
            return;
        }
        struct wl_surface *surface = static_cast<wl_surface *>(
            QGuiApplication::platformNativeInterface()->nativeResourceForWindow(
                QStringLiteral("surface").toLocal8Bit(),
                window->windowHandle()));

        wl_fixed_t dx = wl_fixed_from_int(640);
        wl_fixed_t dy = wl_fixed_from_int(0);

        wl_callback *callback = control.set_xwindow_position_relative(wid, surface, dx, dy);
        if (!callback) {
            qCritical() << "Failed to send set_xwindow_position_relative request!";
            return;
        }
        wl_callback_add_listener(callback, &callback_listener, nullptr);
        qWarning() << "Setting xwindow position relative, wait for result...";
    });
    timer.start(1000);

    return app.exec();
}

#include "main.moc"

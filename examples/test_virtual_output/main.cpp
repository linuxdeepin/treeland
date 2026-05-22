// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualclient.h"

#include <QApplication>

// ./test-virtual-output HDMI-A-1 VGA-1

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);

    // Print help tip if no screen names provided via argv
    if (argc < 3) {
        qInfo() << "Tip: Specify screen names to clone:";
        qInfo() << "  ./test-virtual-output HDMI-A-1 VGA-1";
    }

    VirtualClient client(argc, argv);

    return app.exec();
}

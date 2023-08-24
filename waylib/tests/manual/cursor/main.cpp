// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "widget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");

    QApplication a(argc, argv);
    Widget w;
    w.resize(500, 500);
    w.show();
    return a.exec();
}

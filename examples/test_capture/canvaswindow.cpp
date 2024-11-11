// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "canvaswindow.h"

#include <private/qwaylandwindow_p.h>

#include <qwayland-xdg-shell.h>

CanvasWindow::CanvasWindow()
{
    setFlag(Qt::WindowTransparentForInput);
}

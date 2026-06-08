// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "types.h"
#include "platformplugin/qwlrootswindow.h"

#include <QWindow>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace QW {

bool RenderWindow::beforeDisposeEventFilter(QWindow *window, QEvent *event)
{
    if (auto qwRenderWindow = static_cast<QWlrootsRenderWindow*>(window->handle())) {
        if (qwRenderWindow->beforeDisposeEventFilter(event))
            return true;
    }
    return false;
}

bool RenderWindow::afterDisposeEventFilter(QWindow *window, QEvent *event)
{
    if (auto qwRenderWindow = static_cast<QWlrootsRenderWindow*>(window->handle())) {
        if (qwRenderWindow->afterDisposeEventFilter(event))
            return true;
    }
    return false;
}

}

WAYLIB_SERVER_END_NAMESPACE

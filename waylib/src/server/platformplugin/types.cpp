// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "types.h"
#include "platformplugin/qwlrootswindow.h"

#include <QWindow>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace QW {

bool RenderWindow::eventFilter(QWindow *window, QEvent *event)
{
    if (auto qwRenderWindow = static_cast<QWlrootsRenderWindow*>(window->handle())) {
        if (qwRenderWindow->eventFilter(event))
            return true;
    }
    return false;
}

}

WAYLIB_SERVER_END_NAMESPACE

// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "toolwindow.h"

ToolWindow::ToolWindow() { }

QQuickWindow *ToolWindow::parent() const
{
    return qobject_cast<QQuickWindow *>(QWindow::parent());
}

void ToolWindow::setParent(QQuickWindow *w)
{
    QWindow::setParent(w);
    Q_EMIT parentChanged();
}

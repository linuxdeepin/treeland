// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootswindow.h"
#include "qwlrootscreen.h"
#include "qwlrootsintegration.h"

#include <QCoreApplication>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface_p.h>
#include <private/qguiapplication_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

QWlrootsOutputWindow::QWlrootsOutputWindow(QWindow *window)
    : QPlatformWindow(window)
{

}

void QWlrootsOutputWindow::initialize()
{
    auto onGeometryChanged = [this] {
        const QRect newGeo = geometry();
        QWindowSystemInterface::handleGeometryChange(window(), newGeo);
    };

    QObject::connect(window(), &QWindow::screenChanged,  window(),[this, onGeometryChanged] (QScreen *newScreen) {
        if (onScreenGeometryConnection)
            QObject::disconnect(onScreenGeometryConnection);
        onScreenGeometryConnection = QObject::connect(newScreen, &QScreen::geometryChanged, window(), onGeometryChanged);
    });

    onScreenGeometryConnection = QObject::connect(screen()->screen(), &QScreen::geometryChanged, window(), onGeometryChanged);
    QMetaObject::invokeMethod(window(), onGeometryChanged, Qt::QueuedConnection);
}

QPlatformScreen *QWlrootsOutputWindow::screen() const
{
    return QPlatformWindow::screen();
}

void QWlrootsOutputWindow::setGeometry(const QRect &rect)
{
    auto screen = dynamic_cast<QWlrootsScreen*>(this->screen());
    Q_ASSERT(screen);
    screen->move(rect.topLeft());
}

QRect QWlrootsOutputWindow::geometry() const
{
    return screen()->geometry();
}

WId QWlrootsOutputWindow::winId() const
{
    return reinterpret_cast<WId>(this);
}

qreal QWlrootsOutputWindow::devicePixelRatio() const
{
    return 1.0;
}

WAYLIB_SERVER_END_NAMESPACE

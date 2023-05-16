// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootswindow.h"
#include "qwlrootscreen.h"
#include "qwlrootsintegration.h"

#include <QThread>
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

}

QPlatformScreen *QWlrootsOutputWindow::screen() const
{
    auto screen = dynamic_cast<QWlrootsScreen*>(QPlatformWindow::screen());
    if (!screen) {
        auto instance = QWlrootsIntegration::instance();
        Q_ASSERT(instance);
        if (!instance->screens().isEmpty())
            return instance->screens().first();
    }

    return screen;
}

void QWlrootsOutputWindow::setGeometry(const QRect &rect)
{
    if (m_geometry == rect)
        return;

    m_geometry = rect;

    if (QThread::currentThread() == qApp->thread()) {
        QWindowSystemInterface::handleGeometryChange(window(), rect);
    } else {
        QWindowSystemInterfacePrivate::GeometryChangeEvent event(window(), rect);
        QGuiApplicationPrivate::processWindowSystemEvent(&event);
    }
}

QRect QWlrootsOutputWindow::geometry() const
{
    return m_geometry;
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

// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootswindow.h"
#include "qwlrootscreen.h"
#include "qwlrootsintegration.h"
#include "woutput.h"
#include "winputdevice.h"
#include "wseat.h"
#include "wcursor.h"

#include <qwrenderer.h>
#include <qwoutput.h>

#include <QCoreApplication>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface_p.h>
#include <private/qguiapplication_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

QWlrootsOutputWindow::QWlrootsOutputWindow(QWindow *window)
    : QPlatformWindow(window)
{

}

QWlrootsOutputWindow::~QWlrootsOutputWindow()
{
    if (onScreenChangedConnection)
        QObject::disconnect(onScreenChangedConnection);
    if (onScreenGeometryConnection)
        QObject::disconnect(onScreenGeometryConnection);
}

void QWlrootsOutputWindow::initialize()
{
    auto onGeometryChanged = [this] {
        const QRect newGeo = geometry();
        QWindowSystemInterface::handleGeometryChange(window(), newGeo);
    };

    onScreenChangedConnection = QObject::connect(window(), &QWindow::screenChanged,  window(), [this, onGeometryChanged] (QScreen *newScreen) {
        if (onScreenGeometryConnection)
            QObject::disconnect(onScreenGeometryConnection);
        onScreenGeometryConnection = QObject::connect(newScreen, &QScreen::geometryChanged, window(), onGeometryChanged);
    });

    onScreenGeometryConnection = QObject::connect(screen()->screen(), &QScreen::geometryChanged, window(), onGeometryChanged);
    QMetaObject::invokeMethod(window(), onGeometryChanged, Qt::QueuedConnection);
}

QWlrootsScreen *QWlrootsOutputWindow::qwScreen() const
{
    return dynamic_cast<QWlrootsScreen*>(this->screen());
}

QPlatformScreen *QWlrootsOutputWindow::screen() const
{
    return QPlatformWindow::screen();
}

void QWlrootsOutputWindow::setGeometry(const QRect &rect)
{
    auto screen = qwScreen();
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

QWlrootsRenderWindow::QWlrootsRenderWindow(QWindow *window)
    : QPlatformWindow(window)
{

}

void QWlrootsRenderWindow::initialize()
{

}

void QWlrootsRenderWindow::setGeometry(const QRect &rect)
{
    if (geometry() == rect)
        return;
    QPlatformWindow::setGeometry(rect);
    QWindowSystemInterface::handleGeometryChange(window(), rect);
}

WId QWlrootsRenderWindow::winId() const
{
    return reinterpret_cast<WId>(const_cast<QWlrootsRenderWindow*>(this));
}

qreal QWlrootsRenderWindow::devicePixelRatio() const
{
    return dpr;
}

void QWlrootsRenderWindow::setDevicePixelRatio(qreal dpr)
{
    if (qFuzzyCompare(this->dpr, dpr))
        return;

    this->dpr = dpr;

#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
    QEvent event(QEvent::ScreenChangeInternal);
    QCoreApplication::sendEvent(window(), &event);
#else
    QWindowSystemInterface::handleWindowDevicePixelRatioChanged<QWindowSystemInterface::SynchronousDelivery>(window());
#endif
}

bool QWlrootsRenderWindow::beforeDisposeEventFilter(QEvent *event)
{
    if (event->isInputEvent()) {
        auto ie = static_cast<QInputEvent*>(event);
        auto device = WInputDevice::from(ie->device());
        Q_ASSERT(device);
        Q_ASSERT(device->seat());
        lastActiveCursor = device->seat()->cursor();
        return device->seat()->filterEventBeforeDisposeStage(window(), ie);
    }

    return false;
}

bool QWlrootsRenderWindow::afterDisposeEventFilter(QEvent *event)
{
    if (event->isInputEvent()) {
        auto ie = static_cast<QInputEvent*>(event);
        auto device = WInputDevice::from(ie->device());
        Q_ASSERT(device);
        lastActiveCursor = device->seat()->cursor();
        return device->seat()->filterEventAfterDisposeStage(window(), ie);
    }

    return false;
}

WAYLIB_SERVER_END_NAMESPACE

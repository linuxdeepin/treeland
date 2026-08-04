// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "waylibwindow.h"
#include "waylibscreen.h"
#include "waylibintegration.h"
#include "woutput.h"
#include "winputdevice.h"
#include "wseat.h"
#include "wcursor.h"

#include <QCoreApplication>

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface_p.h>
#include <private/qguiapplication_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

WaylibOutputWindow::WaylibOutputWindow(QWindow *window)
    : QPlatformWindow(window)
{

}

WaylibOutputWindow::~WaylibOutputWindow()
{
    if (onScreenChangedConnection)
        QObject::disconnect(onScreenChangedConnection);
    if (onScreenGeometryConnection)
        QObject::disconnect(onScreenGeometryConnection);
}

void WaylibOutputWindow::initialize()
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

WaylibScreen *WaylibOutputWindow::waylibScreen() const
{
    return dynamic_cast<WaylibScreen*>(this->screen());
}

QPlatformScreen *WaylibOutputWindow::screen() const
{
    return QPlatformWindow::screen();
}

void WaylibOutputWindow::setGeometry(const QRect &rect)
{
    auto screen = waylibScreen();
    Q_ASSERT(screen);
    screen->move(rect.topLeft());
}

QRect WaylibOutputWindow::geometry() const
{
    return screen()->geometry();
}

WId WaylibOutputWindow::winId() const
{
    return reinterpret_cast<WId>(this);
}

qreal WaylibOutputWindow::devicePixelRatio() const
{
    return 1.0;
}

WaylibRenderWindow::WaylibRenderWindow(QWindow *window)
    : QPlatformWindow(window)
{

}

void WaylibRenderWindow::initialize()
{

}

void WaylibRenderWindow::setGeometry(const QRect &rect)
{
    if (geometry() == rect)
        return;
    QPlatformWindow::setGeometry(rect);
    QWindowSystemInterface::handleGeometryChange(window(), rect);
}

WId WaylibRenderWindow::winId() const
{
    return reinterpret_cast<WId>(const_cast<WaylibRenderWindow*>(this));
}

qreal WaylibRenderWindow::devicePixelRatio() const
{
    return dpr;
}

void WaylibRenderWindow::setDevicePixelRatio(qreal dpr)
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

bool WaylibRenderWindow::beforeDisposeEventFilter(QEvent *event)
{
    if (event->isInputEvent()) {
        auto ie = static_cast<QInputEvent*>(event);
        auto seat = WSeat::fromInputEvent(ie);
        if (!seat) return true;
        lastActiveCursor = seat->cursor();
        return seat->filterEventBeforeDisposeStage(window(), ie);
    }

    return false;
}

bool WaylibRenderWindow::afterDisposeEventFilter(QEvent *event)
{
    if (event->isInputEvent()) {
        auto ie = static_cast<QInputEvent*>(event);
        auto seat = WSeat::fromInputEvent(ie);
        if (!seat) return true;
        lastActiveCursor = seat->cursor();
        return seat->filterEventAfterDisposeStage(window(), ie);
    }

    return false;
}

WAYLIB_SERVER_END_NAMESPACE

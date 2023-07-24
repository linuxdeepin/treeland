// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickcursor.h"
#include "private/wcursor_p.h"
#include "woutputrenderwindow.h"
#include "wquickoutputlayout.h"
#include "woutputpositioner.h"
#include "wxcursorimage.h"

#include <qwxcursormanager.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickCursorPrivate : public WCursorPrivate
{
public:
    WQuickCursorPrivate(WQuickCursor *qq);
    ~WQuickCursorPrivate() {
        delete xcursor_manager;
    }

    inline static WQuickCursorPrivate *get(WQuickCursor *qq) {
        return qq->d_func();
    }

    // TODO: Allow add render window in public interface, because
    // maybe a WOutputRenderWindow is no attach output.
    void updateRenderWindows();
    void updateCurrentRenderWindow();
    void setCurrentRenderWindow(WOutputRenderWindow *window);
    void onRenderWindowAdded(WOutputRenderWindow *window);
    void onRenderWindowRemoved(WOutputRenderWindow *window);
    void onCursorPositionChanged();

    void setCursorImageUrl(const QUrl &url);
    void updateXCursorManager();

    inline quint32 getCursorSize() const {
        return qMax(cursorSize.width(), cursorSize.height());
    }

    W_DECLARE_PUBLIC(WQuickCursor)
    bool componentComplete = true;
    QList<WOutputRenderWindow*> renderWindows;
    WOutputRenderWindow *currentRenderWindow = nullptr;
    QString xcursorThemeName;
    QSize cursorSize = QSize(24, 24);
};

WQuickCursorPrivate::WQuickCursorPrivate(WQuickCursor *qq)
    : WCursorPrivate(qq)
{

}

void WQuickCursorPrivate::updateRenderWindows()
{
    QList<WOutputRenderWindow*> newList;

    W_Q(WQuickCursor);
    for (auto o : q->layout()->outputs()) {
        QObject::connect(o, SIGNAL(windowChanged(QQuickWindow*)), q, SLOT(updateRenderWindows()));

        auto renderWindow = qobject_cast<WOutputRenderWindow*>(o->window());
        if (!renderWindow)
            continue;
        if (newList.contains(renderWindow))
            continue;
        newList.append(renderWindow);

        if (!renderWindows.contains(renderWindow)) {
            renderWindows.append(renderWindow);
            onRenderWindowAdded(renderWindow);
        }
    }

    for (int i = 0; i < renderWindows.count(); ++i) {
        auto window = renderWindows.at(i);
        if (!newList.contains(window)) {
            renderWindows.removeAt(i);
            --i;
            onRenderWindowRemoved(window);
        }
    }

    updateCurrentRenderWindow();
}

void WQuickCursorPrivate::updateCurrentRenderWindow()
{
    if (renderWindows.isEmpty()) {
        setCurrentRenderWindow(nullptr);
        return;
    }

    W_Q(WQuickCursor);
    const QPoint pos = q->position().toPoint();
    if (currentRenderWindow && currentRenderWindow->geometry().contains(pos))
        return;

    for (auto window : renderWindows) {
        // ###: maybe should use QGuiApplication::topLevelAt
        if (window->geometry().contains(pos)) {
            setCurrentRenderWindow(window);
            break;
        }
    }
}

void WQuickCursorPrivate::setCurrentRenderWindow(WOutputRenderWindow *window)
{
    if (currentRenderWindow == window)
        return;
    currentRenderWindow = window;
    q_func()->WCursor::setEventWindow(window);
}

void WQuickCursorPrivate::onRenderWindowAdded(WOutputRenderWindow *window)
{
    Q_UNUSED(window);
}

void WQuickCursorPrivate::onRenderWindowRemoved(WOutputRenderWindow *window)
{
    Q_UNUSED(window);
}

void WQuickCursorPrivate::onCursorPositionChanged()
{
    updateCurrentRenderWindow();
}

void WQuickCursorPrivate::updateXCursorManager()
{
    if (xcursor_manager) delete xcursor_manager;
    const char *cursor_theme = xcursorThemeName.isEmpty() ? nullptr : qPrintable(xcursorThemeName);
    auto xm = QWXCursorManager::create(cursor_theme, getCursorSize());
    q_func()->setXCursorManager(xm);
}

WQuickCursor::WQuickCursor(QObject *parent)
    : WCursor(*new WQuickCursorPrivate(this), parent)
{

}

WQuickCursor::~WQuickCursor()
{

}

WQuickOutputLayout *WQuickCursor::layout() const
{
    return static_cast<WQuickOutputLayout*>(WCursor::layout());
}

void WQuickCursor::setLayout(WQuickOutputLayout *layout)
{
    if (auto layout = this->layout()) {
        disconnect(layout, SIGNAL(outputsChanged()), this, SLOT(updateRenderWindows()));
    }

    WCursor::setLayout(layout);

    if (layout) {
        connect(layout, SIGNAL(outputsChanged()), this, SLOT(updateRenderWindows()));
    }

    W_D(WQuickCursor);

    if (d->componentComplete) {
        d->updateXCursorManager();
        d->updateRenderWindows();
    }
}

WOutputRenderWindow *WQuickCursor::currentRenderWindow() const
{
    W_DC(WQuickCursor);
    return d->currentRenderWindow;
}

QString WQuickCursor::themeName() const
{
    W_DC(WQuickCursor);
    return d->xcursorThemeName;
}

void WQuickCursor::setThemeName(const QString &name)
{
    W_D(WQuickCursor);

    if (d->xcursorThemeName == name)
        return;
    d->xcursorThemeName = name;
    QMetaObject::invokeMethod(this, "updateXCursorManager", Qt::QueuedConnection);
}

QSize WQuickCursor::size() const
{
    W_DC(WQuickCursor);
    return d->cursorSize;
}

void WQuickCursor::setSize(const QSize &size)
{
    W_D(WQuickCursor);

    if (d->cursorSize == size)
        return;
    d->cursorSize = size;
    QMetaObject::invokeMethod(this, "updateXCursorManager", Qt::QueuedConnection);
}

void WQuickCursor::move(QW_NAMESPACE::QWInputDevice *device, const QPointF &delta)
{
    W_D(WQuickCursor);
    WCursor::move(device, delta);
    d->onCursorPositionChanged();
}

void WQuickCursor::setPosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos)
{
    W_D(WQuickCursor);
    WCursor::setPosition(device, pos);
    d->onCursorPositionChanged();
}

bool WQuickCursor::setPositionWithChecker(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos)
{
    W_D(WQuickCursor);

    if (!WCursor::setPositionWithChecker(device, pos))
        return false;

    d->onCursorPositionChanged();
    return true;
}

void WQuickCursor::setScalePosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &ratio)
{
    W_D(WQuickCursor);
    WCursor::setScalePosition(device, ratio);
    d->onCursorPositionChanged();
}

void WQuickCursor::classBegin()
{
    W_D(WQuickCursor);
    d->componentComplete = false;
}

void WQuickCursor::componentComplete()
{
    W_D(WQuickCursor);

    if (layout()) {
        d->updateXCursorManager();
        d->updateRenderWindows();
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquickcursor.cpp"

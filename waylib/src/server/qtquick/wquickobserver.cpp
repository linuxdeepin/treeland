// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickobserver.h"

#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WQuickObserverPrivate : public QQuickItemPrivate
{
public:
    WQuickObserverPrivate()
    {

    }

    bool transformChanged(QQuickItem *transformedItem) override {
        Q_Q(WQuickObserver);

        if (!inDestructor) {
            Q_EMIT q->transformChanged(transformedItem);
            Q_EMIT q->maybeScenePositionChanged();
            Q_EMIT q->maybeGlobalPositionChanged();
        }

        return QQuickItemPrivate::transformChanged(transformedItem);
    }

    qreal getImplicitWidth() const override {
        return q_func()->getImplicitWidth();
    }
    qreal getImplicitHeight() const override {
        return q_func()->getImplicitHeight();
    }

    W_DECLARE_PUBLIC(WQuickObserver)

    QMetaObject::Connection windowXChangeConnection;
    QMetaObject::Connection windowYChangeConnection;

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    bool inDestructor = false;
#endif
};


WQuickObserver::WQuickObserver(QQuickItem *parent)
    : QQuickItem(*new WQuickObserverPrivate(), parent)
{
    setFlag(ItemObservesViewport);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
WQuickObserver::~WQuickObserver()
{
    Q_D(WQuickObserver);
    d->inDestructor = true;
}
#endif

const QPointF WQuickObserver::globalPosition() const
{
    if (!parentItem())
        return position();
    return parentItem()->mapToGlobal(position());
}

const QPointF WQuickObserver::scenePosition() const
{
    if (!parentItem())
        return position();
    return parentItem()->mapToScene(position());
}

void WQuickObserver::componentComplete()
{
    Q_D(WQuickObserver);

    if (d->window) {
        d->windowXChangeConnection = connect(d->window, &QQuickWindow::xChanged,
                                             this, &WQuickObserver::maybeGlobalPositionChanged);
        d->windowYChangeConnection = connect(d->window, &QQuickWindow::yChanged,
                                             this, &WQuickObserver::maybeGlobalPositionChanged);
    }

    QQuickItem::componentComplete();
}

void WQuickObserver::releaseResources()
{
    Q_D(WQuickObserver);

    if (d->windowXChangeConnection)
        disconnect(d->windowXChangeConnection);
    if (d->windowYChangeConnection)
        disconnect(d->windowYChangeConnection);

    QQuickItem::releaseResources();
}

void WQuickObserver::itemChange(ItemChange change, const ItemChangeData &data)
{
    Q_D(WQuickObserver);

    if (d->inDestructor)
        return QQuickItem::itemChange(change, data);

    if (change == ItemChange::ItemParentHasChanged) {
        Q_EMIT maybeScenePositionChanged();
        Q_EMIT maybeGlobalPositionChanged();
    } else if (change == ItemChange::ItemSceneChange) {
        Q_D(WQuickObserver);

        if (d->windowXChangeConnection)
            disconnect(d->windowXChangeConnection);
        if (d->windowYChangeConnection)
            disconnect(d->windowYChangeConnection);

        if (data.window) {
            d->windowXChangeConnection = connect(data.window, &QQuickWindow::xChanged,
                                                 this, &WQuickObserver::maybeGlobalPositionChanged);
            d->windowYChangeConnection = connect(data.window, &QQuickWindow::yChanged,
                                                 this, &WQuickObserver::maybeGlobalPositionChanged);
        }

        Q_EMIT maybeGlobalPositionChanged();
    }

    QQuickItem::itemChange(change, data);
}

void WQuickObserver::privateImplicitWidthChanged()
{
    d_func()->implicitWidthChanged();
}

void WQuickObserver::privateImplicitHeightChanged()
{
    d_func()->implicitHeightChanged();
}

qreal WQuickObserver::getImplicitWidth() const
{
    return d_func()->QQuickItemPrivate::getImplicitWidth();
}

qreal WQuickObserver::getImplicitHeight() const
{
    return d_func()->QQuickItemPrivate::getImplicitHeight();
}

WAYLIB_SERVER_END_NAMESPACE

// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickwaylandserver.h"
#include "private/wserver_p.h"

#include <private/qobject_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickWaylandServerInterfacePrivate : public QObjectPrivate
{
public:
    Q_DECLARE_PUBLIC(WQuickWaylandServerInterface)

    bool isPolished = false;
    WSocket *ownsSocket = nullptr;
};

class WQuickWaylandServerPrivate : public WServerPrivate
{
public:
    WQuickWaylandServerPrivate(WQuickWaylandServer *qq)
        : WServerPrivate(qq) {}

    static WQuickWaylandServerPrivate *get(WQuickWaylandServer *qq) {
        return qq->d_func();
    }

    // interfaces property
    static void interfaces_append(QQmlListProperty<WQuickWaylandServerInterface> *, WQuickWaylandServerInterface *);
    static qsizetype interfaces_count(QQmlListProperty<WQuickWaylandServerInterface> *);
    static WQuickWaylandServerInterface *interfaces_at(QQmlListProperty<WQuickWaylandServerInterface> *, qsizetype);
    static void interfaces_clear(QQmlListProperty<WQuickWaylandServerInterface> *);
    static void interfaces_removeLast(QQmlListProperty<WQuickWaylandServerInterface> *);

    QQmlListProperty<WQuickWaylandServerInterface> interfaces() const;

    // children property
    static void children_append(QQmlListProperty<QObject> *, QObject *);
    static qsizetype children_count(QQmlListProperty<QObject> *);
    static QObject *children_at(QQmlListProperty<QObject> *, qsizetype);
    static void children_clear(QQmlListProperty<QObject> *);
    static void children_removeLast(QQmlListProperty<QObject> *);

    QQmlListProperty<QObject> children() const;

    W_DECLARE_PUBLIC(WQuickWaylandServer)
    bool componentComplete = true;
    QList<WQuickWaylandServerInterface*> m_interfaces;
};

void WQuickWaylandServerPrivate::interfaces_append(QQmlListProperty<WQuickWaylandServerInterface> *prop, WQuickWaylandServerInterface *i)
{
    auto q = static_cast<WQuickWaylandServer*>(prop->object);
    auto self = get(q);

    self->m_interfaces.append(i);
    i->setParent(q);

    if (self->componentComplete) {
        i->create();
        QMetaObject::invokeMethod(i, &WQuickWaylandServerInterface::polish, Qt::QueuedConnection);
    }

    Q_EMIT q->interfacesChanged();
}

qsizetype WQuickWaylandServerPrivate::interfaces_count(QQmlListProperty<WQuickWaylandServerInterface> *prop)
{
    auto q = static_cast<WQuickWaylandServer*>(prop->object);
    auto self = get(q);

    return self->m_interfaces.count();
}

WQuickWaylandServerInterface *WQuickWaylandServerPrivate::interfaces_at(QQmlListProperty<WQuickWaylandServerInterface> *prop, qsizetype index)
{
    auto q = static_cast<WQuickWaylandServer*>(prop->object);
    auto self = get(q);

    return self->m_interfaces.at(index);
}

void WQuickWaylandServerPrivate::interfaces_clear(QQmlListProperty<WQuickWaylandServerInterface> *prop)
{
    auto q = static_cast<WQuickWaylandServer*>(prop->object);
    auto self = get(q);

    for (WQuickWaylandServerInterface *interface : self->m_interfaces)
        interface->setParent(nullptr);
    self->m_interfaces.clear();
    Q_EMIT q->interfacesChanged();
}

void WQuickWaylandServerPrivate::interfaces_removeLast(QQmlListProperty<WQuickWaylandServerInterface> *prop)
{
    auto q = static_cast<WQuickWaylandServer*>(prop->object);
    auto self = get(q);

    if (self->m_interfaces.isEmpty())
        return;

    self->m_interfaces.takeLast()->setParent(nullptr);
    Q_EMIT q->interfacesChanged();
}

QQmlListProperty<WQuickWaylandServerInterface> WQuickWaylandServerPrivate::interfaces() const
{
    QQmlListProperty<WQuickWaylandServerInterface> result;
    result.object = const_cast<WQuickWaylandServer*>(q_func());
    result.append = WQuickWaylandServerPrivate::interfaces_append;
    result.count = WQuickWaylandServerPrivate::interfaces_count;
    result.at = WQuickWaylandServerPrivate::interfaces_at;
    result.clear = WQuickWaylandServerPrivate::interfaces_clear;
    result.removeLast = WQuickWaylandServerPrivate::interfaces_removeLast;
    return result;
}

void WQuickWaylandServerPrivate::children_append(QQmlListProperty<QObject> *prop, QObject *o)
{
    auto q = static_cast<WQuickWaylandServer*>(prop->object);

    o->setParent(q);
}

qsizetype WQuickWaylandServerPrivate::children_count(QQmlListProperty<QObject> *prop)
{
    auto q = static_cast<WQuickWaylandServer*>(prop->object);

    return q->children().size();
}

QObject *WQuickWaylandServerPrivate::children_at(QQmlListProperty<QObject> *prop, qsizetype i)
{
    auto q = static_cast<WQuickWaylandServer*>(prop->object);
    return q->children().at(i);
}

QQmlListProperty<QObject> WQuickWaylandServerPrivate::children() const
{
    QQmlListProperty<QObject> result;
    result.object = const_cast<WQuickWaylandServer*>(q_func());
    result.append = WQuickWaylandServerPrivate::children_append;
    result.count = WQuickWaylandServerPrivate::children_count;
    result.at = WQuickWaylandServerPrivate::children_at;
    return result;
}

WQuickWaylandServerInterface::WQuickWaylandServerInterface(QObject *parent)
    : QObject(*new WQuickWaylandServerInterfacePrivate(), parent)
{

}

WQuickWaylandServer *WQuickWaylandServerInterface::server() const
{
    return qobject_cast<WQuickWaylandServer*>(parent());
}

bool WQuickWaylandServerInterface::isPolished() const
{
    Q_D(const WQuickWaylandServerInterface);
    return d->isPolished;
}

WSocket *WQuickWaylandServerInterface::ownsSocket() const
{
    Q_D(const WQuickWaylandServerInterface);
    return d->ownsSocket;
}

void WQuickWaylandServerInterface::setOwnsSocket(WSocket *socket)
{
    Q_D(WQuickWaylandServerInterface);
    if (d->ownsSocket == socket)
        return;
    d->ownsSocket = socket;
    Q_EMIT ownsSocketChanged();

    ownsSocketChange();
}

void WQuickWaylandServerInterface::create()
{
    Q_EMIT beforeCreate();
}

void WQuickWaylandServerInterface::polish()
{
    Q_D(WQuickWaylandServerInterface);
    d->isPolished = true;
    Q_EMIT afterPolish();
}

void WQuickWaylandServerInterface::ownsSocketChange()
{

}

WQuickWaylandServer::WQuickWaylandServer(QObject *parent)
    : WServer(*new WQuickWaylandServerPrivate(this), parent)
{

}

void WQuickWaylandServer::classBegin()
{
    W_D(WQuickWaylandServer);
    d->componentComplete = false;
}

void WQuickWaylandServer::componentComplete()
{
    W_D(WQuickWaylandServer);

    start();
    for (auto i : d->m_interfaces)
        i->create();

    for (auto i : d->m_interfaces)
        i->polish();

    d->componentComplete = true;

    Q_EMIT started();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquickwaylandserver.cpp"

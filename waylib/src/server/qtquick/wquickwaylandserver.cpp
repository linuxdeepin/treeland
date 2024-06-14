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

    void updateTargetClients();

    // targetClients property
    static void targetClients_append(QQmlListProperty<WClient> *, WClient *);
    static qsizetype targetClients_count(QQmlListProperty<WClient> *);
    static WClient *targetClients_at(QQmlListProperty<WClient> *, qsizetype);
    static void targetClients_clear(QQmlListProperty<WClient> *);
    static void targetClients_removeLast(QQmlListProperty<WClient> *);
    static void targetClients_replace(QQmlListProperty<WClient> *, qsizetype, WClient *);

    QQmlListProperty<WClient> targetClients() const;

    bool isPolished = false;
    WSocket *targetSocket = nullptr;
    QList<WClient*> m_targetClients;
    bool exclusionTargetClients = false;
    WServerInterface *interface = nullptr;
};

void WQuickWaylandServerInterfacePrivate::updateTargetClients()
{
    if (!interface)
        return;
    interface->setTargetClients(m_targetClients, exclusionTargetClients);
}

void WQuickWaylandServerInterfacePrivate::targetClients_append(QQmlListProperty<WClient> *prop, WClient *i)
{
    auto q = static_cast<WQuickWaylandServerInterface*>(prop->object);
    auto self = q->d_func();

    self->m_targetClients.append(i);
    self->updateTargetClients();

    Q_EMIT q->targetClientsChanged();
}

qsizetype WQuickWaylandServerInterfacePrivate::targetClients_count(QQmlListProperty<WClient> *prop)
{
    auto q = static_cast<WQuickWaylandServerInterface*>(prop->object);
    auto self = q->d_func();

    return self->m_targetClients.size();
}

WClient *WQuickWaylandServerInterfacePrivate::targetClients_at(QQmlListProperty<WClient> *prop, qsizetype index)
{
    auto q = static_cast<WQuickWaylandServerInterface*>(prop->object);
    auto self = q->d_func();

    return self->m_targetClients.at(index);
}

void WQuickWaylandServerInterfacePrivate::targetClients_clear(QQmlListProperty<WClient> *prop)
{
    auto q = static_cast<WQuickWaylandServerInterface*>(prop->object);
    auto self = q->d_func();

    if (self->m_targetClients.isEmpty())
        return;

    self->m_targetClients.clear();
    self->updateTargetClients();

    Q_EMIT q->targetClientsChanged();
}

void WQuickWaylandServerInterfacePrivate::targetClients_removeLast(QQmlListProperty<WClient> *prop)
{
    auto q = static_cast<WQuickWaylandServerInterface*>(prop->object);
    auto self = q->d_func();

    self->m_targetClients.removeLast();
    self->updateTargetClients();

    Q_EMIT q->targetClientsChanged();
}

void WQuickWaylandServerInterfacePrivate::targetClients_replace(QQmlListProperty<WClient> *prop, qsizetype index, WClient *client)
{
    auto q = static_cast<WQuickWaylandServerInterface*>(prop->object);
    auto self = q->d_func();
    self->m_targetClients.replace(index, client);

    Q_EMIT q->targetClientsChanged();
}

QQmlListProperty<WClient> WQuickWaylandServerInterfacePrivate::targetClients() const
{
    QQmlListProperty<WClient> result;
    result.object = const_cast<WQuickWaylandServerInterface*>(q_func());
    result.append = WQuickWaylandServerInterfacePrivate::targetClients_append;
    result.count = WQuickWaylandServerInterfacePrivate::targetClients_count;
    result.at = WQuickWaylandServerInterfacePrivate::targetClients_at;
    result.clear = WQuickWaylandServerInterfacePrivate::targetClients_clear;
    result.removeLast = WQuickWaylandServerInterfacePrivate::targetClients_removeLast;
    result.replace = WQuickWaylandServerInterfacePrivate::targetClients_replace;
    return result;
}

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
        i->doCreate();
        QMetaObject::invokeMethod(i, &WQuickWaylandServerInterface::doPolish, Qt::QueuedConnection);
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

WSocket *WQuickWaylandServerInterface::targetSocket() const
{
    Q_D(const WQuickWaylandServerInterface);
    return d->targetSocket;
}

void WQuickWaylandServerInterface::setTargetSocket(WSocket *socket)
{
    Q_D(WQuickWaylandServerInterface);
    if (d->targetSocket == socket)
        return;
    d->targetSocket = socket;
    Q_EMIT targetSocketChanged();

    if (d->interface)
        d->interface->setTargetSocket(socket, false);
}

bool WQuickWaylandServerInterface::exclusionTargetClients() const
{
    Q_D(const WQuickWaylandServerInterface);
    return d->exclusionTargetClients;
}

void WQuickWaylandServerInterface::setExclusionTargetClients(bool newExclusionTargetClients)
{
    Q_D(WQuickWaylandServerInterface);
    if (d->exclusionTargetClients == newExclusionTargetClients)
        return;
    d->exclusionTargetClients = newExclusionTargetClients;
    d->updateTargetClients();

    Q_EMIT exclusionTargetClientsChanged();
}

WServerInterface *WQuickWaylandServerInterface::create()
{
    return nullptr;
}

void WQuickWaylandServerInterface::polish()
{

}

void WQuickWaylandServerInterface::doCreate()
{
    Q_D(WQuickWaylandServerInterface);
    Q_ASSERT(!d->interface);
    Q_EMIT beforeCreate();
    d->interface = create();

    if (!d->interface)
        return;

    d->interface->setTargetSocket(targetSocket(), false);
    if (!d->m_targetClients.isEmpty())
        d->updateTargetClients();

    if (!server()->interfaceList().contains(d->interface))
        server()->attach(d->interface);
}

void WQuickWaylandServerInterface::doPolish()
{
    Q_D(WQuickWaylandServerInterface);

    polish();

    d->isPolished = true;
    Q_EMIT afterPolish();
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
        i->doCreate();

    for (auto i : d->m_interfaces)
        i->doPolish();

    d->componentComplete = true;

    Q_EMIT started();
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquickwaylandserver.cpp"

// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wqmlcomconent_p.h"

#include <private/qqmlcomponent_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

WQmlComponent::WQmlComponent(QObject *parent)
    : QObject{parent}
{

}

QQmlComponent *WQmlComponent::delegate() const
{
    return m_delegate;
}

void WQmlComponent::setDelegate(QQmlComponent *newDelegate)
{
    if (m_delegate == newDelegate)
        return;
    m_delegate = newDelegate;
    Q_EMIT delegateChanged();
}

QObject *WQmlComponent::create(QObject *parent, const QVariantMap &initialProperties)
{
    return create(parent, nullptr, initialProperties);
}

QObject *WQmlComponent::create(QObject *parent, QObject *owner, const QVariantMap &initialProperties)
{
    if (!m_delegate || m_delegate->status() == QQmlComponent::Null)
        return nullptr;

    Q_ASSERT(m_delegate->status() == QQmlComponent::Ready);
    auto d = QQmlComponentPrivate::get(m_delegate);
    if (d->state.isCompletePending()) {
        qmlEngine(this)->throwError(QJSValue::GenericError, "The last 'create' request is pending...");
        return nullptr;
    }
    auto obj = d->createWithProperties(parent, initialProperties, qmlContext(this));

    if (obj && owner) {
        m_ownerToObjects.insert(owner, obj);
    }

    return obj;
}

void WQmlComponent::destroyIfOwnerIs(QObject *owner)
{
    auto objs = m_ownerToObjects.values(owner);
    for (auto o : objs)
        o->deleteLater();
    m_ownerToObjects.remove(owner);
}

WAYLIB_SERVER_END_NAMESPACE

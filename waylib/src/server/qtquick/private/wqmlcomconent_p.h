// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QQmlComponent>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WQmlComponent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged FINAL)
    QML_NAMED_ELEMENT(WaylibComponent)
    Q_CLASSINFO("DefaultProperty", "delegate")

public:
    explicit WQmlComponent(QObject *parent = nullptr);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *newDelegate);

public Q_SLOTS:
    QObject *create(QObject *parent, const QVariantMap &initialProperties);
    QObject *create(QObject *parent, QObject *owner, const QVariantMap &initialProperties);
    void destroyIfOwnerIs(QObject *owner);

Q_SIGNALS:
    void delegateChanged();

private:
    QQmlComponent *m_delegate = nullptr;
    QMultiMap<QObject*, QObject*> m_ownerToObjects;
};

WAYLIB_SERVER_END_NAMESPACE

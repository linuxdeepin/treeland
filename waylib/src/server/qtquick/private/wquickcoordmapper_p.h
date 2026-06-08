// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QQuickItem>

Q_MOC_INCLUDE("wquickobserver.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickObserver;
class WQuickCoordMapper;
class WAYLIB_SERVER_EXPORT WQuickCoordMapperHelper : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit WQuickCoordMapperHelper(QQuickItem *target);

public Q_SLOTS:
    WQuickCoordMapper *get(WQuickObserver *target);

private:
    QQuickItem *m_target;
    QList<WQuickCoordMapper*> list;
};

class WAYLIB_SERVER_EXPORT WQuickCoordMapperAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WQuickCoordMapperHelper* helper READ helper NOTIFY helperChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WQuickCoordMapperAttached(QQuickItem *target);

    WQuickCoordMapperHelper *helper() const;

Q_SIGNALS:
    void helperChanged();

private:
    QQuickItem *m_target;
};

class WQuickCoordMapperPrivate;
class WAYLIB_SERVER_EXPORT WQuickCoordMapper : public QQuickItem
{
    friend class WQuickCoordMapperHelper;
    Q_OBJECT
    Q_DECLARE_PRIVATE(WQuickCoordMapper)
    QML_ATTACHED(WQuickCoordMapperAttached)
    QML_NAMED_ELEMENT(CoordMapper)
    QML_UNCREATABLE("Only using for attached property")

public:
    explicit WQuickCoordMapper(WQuickObserver *target, QQuickItem *parent);

    static WQuickCoordMapperAttached *qmlAttachedProperties(QObject *target);

private:
    void updatePosition();
};

WAYLIB_SERVER_END_NAMESPACE

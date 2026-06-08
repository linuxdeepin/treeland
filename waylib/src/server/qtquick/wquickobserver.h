// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickObserverPrivate;
class WAYLIB_SERVER_EXPORT WQuickObserver : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WQuickObserver)
    Q_PROPERTY(QPointF globalPosition READ globalPosition NOTIFY maybeGlobalPositionChanged)
    Q_PROPERTY(QPointF scenePosition READ scenePosition NOTIFY maybeScenePositionChanged)

    QML_NAMED_ELEMENT(Observer)

public:
    explicit WQuickObserver(QQuickItem *parent = nullptr);

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    ~WQuickObserver();
#endif

    const QPointF globalPosition() const;
    const QPointF scenePosition() const;

Q_SIGNALS:
    void maybeGlobalPositionChanged();
    void maybeScenePositionChanged();
    void transformChanged(QQuickItem *transformedItem);

protected:
    void componentComplete() override;
    void releaseResources() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;

    void privateImplicitWidthChanged();
    void privateImplicitHeightChanged();

    virtual qreal getImplicitWidth() const;
    virtual qreal getImplicitHeight() const;
};

WAYLIB_SERVER_END_NAMESPACE

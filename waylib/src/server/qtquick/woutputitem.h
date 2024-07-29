// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wquickobserver.h>

#include <QQuickItem>

Q_MOC_INCLUDE(<wquickoutputlayout.h>)
Q_MOC_INCLUDE(<woutputitem_p.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickSeat;
class WOutput;
class WQuickOutputLayout;
class WOutputItemAttached;
class WOutputItemPrivate;
class WAYLIB_SERVER_EXPORT WOutputItem : public WQuickObserver, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutputItem)
    Q_PROPERTY(WOutput* output READ output WRITE setOutput NOTIFY outputChanged REQUIRED)
    Q_PROPERTY(WQuickOutputLayout* layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
    Q_PROPERTY(QQmlComponent* cursorDelegate READ cursorDelegate WRITE setCursorDelegate NOTIFY cursorDelegateChanged)
    Q_PROPERTY(QList<QQuickItem*> cursorItems READ cursorItems NOTIFY cursorItemsChanged)
    QML_NAMED_ELEMENT(OutputItem)
    QML_ATTACHED(WOutputItemAttached)

public:
    explicit WOutputItem(QQuickItem *parent = nullptr);
    ~WOutputItem();

    static WOutputItemAttached *qmlAttachedProperties(QObject *target);
    static WOutputItem *getOutputItem(WOutput *output);

    WOutput *output() const;
    void setOutput(WOutput *newOutput);

    WQuickOutputLayout *layout() const;
    void setLayout(WQuickOutputLayout *layout);

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal newDevicePixelRatio);

    QQmlComponent *cursorDelegate() const;
    void setCursorDelegate(QQmlComponent *delegate);

    QList<QQuickItem*> cursorItems() const;

Q_SIGNALS:
    void outputChanged();
    void layoutChanged();
    void devicePixelRatioChanged();
    void seatChanged();
    void cursorDelegateChanged();
    void cursorItemsChanged();

private:
    void classBegin() override;
    void componentComplete() override;
    void releaseResources() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;

    qreal getImplicitWidth() const override;
    qreal getImplicitHeight() const override;

    W_PRIVATE_SLOT(void updateCursors())
};

WAYLIB_SERVER_END_NAMESPACE

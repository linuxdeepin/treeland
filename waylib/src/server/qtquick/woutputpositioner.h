// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wquickobserver.h>

#include <QQuickItem>

Q_MOC_INCLUDE(<wquickoutputlayout.h>)
Q_MOC_INCLUDE(<woutputpositioner_p.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WQuickOutputLayout;
class WOutputPositionerAttached;
class WOutputPositionerPrivate;
class WAYLIB_SERVER_EXPORT WOutputPositioner : public WQuickObserver, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutputPositioner)
    Q_PROPERTY(WOutput* output READ output WRITE setOutput REQUIRED)
    Q_PROPERTY(WQuickOutputLayout* layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
    QML_NAMED_ELEMENT(OutputPositioner)
    QML_ATTACHED(WOutputPositionerAttached)

public:
    explicit WOutputPositioner(QQuickItem *parent = nullptr);
    ~WOutputPositioner();

    static WOutputPositionerAttached *qmlAttachedProperties(QObject *target);

    WOutput *output() const;
    void setOutput(WOutput *newOutput);

    WQuickOutputLayout *layout() const;
    void setLayout(WQuickOutputLayout *layout);

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal newDevicePixelRatio);

Q_SIGNALS:
    void layoutChanged();
    void devicePixelRatioChanged();

private:
    void componentComplete() override;
    void releaseResources() override;

    qreal getImplicitWidth() const override;
    qreal getImplicitHeight() const override;
};

WAYLIB_SERVER_END_NAMESPACE

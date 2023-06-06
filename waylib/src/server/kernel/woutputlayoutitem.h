// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QQuickItem>

Q_MOC_INCLUDE("woutputviewport.h")

QW_BEGIN_NAMESPACE
class QWOutputLayout;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewport;
class WOutputLayoutItemPrivate;
class WAYLIB_SERVER_EXPORT WOutputLayoutItem : public QQuickItem, public WObject
{
    W_DECLARE_PRIVATE(WOutputLayoutItem)
    Q_OBJECT
    QML_NAMED_ELEMENT(OutputLayoutItem)
    Q_PROPERTY(QList<WOutputViewport*> outputs READ outputs NOTIFY outputsChanged)

public:
    explicit WOutputLayoutItem(QQuickItem *parent = nullptr);

    QList<WOutputViewport*> outputs() const;

Q_SIGNALS:
    void outputsChanged();
    void enterOutput(WOutputViewport *output);
    void leaveOutput(WOutputViewport *output);

private:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
};

WAYLIB_SERVER_END_NAMESPACE

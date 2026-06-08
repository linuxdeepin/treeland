// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wquickobserver.h>
#include <qwglobal.h>

#include <QQuickItem>

Q_MOC_INCLUDE("woutput.h")
Q_MOC_INCLUDE(<wquickoutputlayout.h>)

QW_BEGIN_NAMESPACE
class QWOutputLayout;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WQuickOutputLayout;
class WOutputLayoutItemPrivate;
class WAYLIB_SERVER_EXPORT WOutputLayoutItem : public WQuickObserver, public WObject
{
    W_DECLARE_PRIVATE(WOutputLayoutItem)
    Q_OBJECT
    QML_NAMED_ELEMENT(OutputLayoutItem)
    Q_PROPERTY(WQuickOutputLayout* layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(QList<WOutput*> outputs READ outputs RESET resetOutput NOTIFY outputsChanged)

public:
    explicit WOutputLayoutItem(QQuickItem *parent = nullptr);

    QList<WOutput*> outputs() const;

    WQuickOutputLayout *layout() const;
    void setLayout(WQuickOutputLayout *newLayout);
    void resetOutput();

Q_SIGNALS:
    void outputsChanged();
    void enterOutput(WOutput *output);
    void leaveOutput(WOutput *output);

    void layoutChanged();

private:
    void componentComplete() override;

    W_PRIVATE_SLOT(void updateOutputs())
};

WAYLIB_SERVER_END_NAMESPACE

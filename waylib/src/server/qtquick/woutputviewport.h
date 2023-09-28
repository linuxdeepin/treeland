// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputViewportPrivate;
class WAYLIB_SERVER_EXPORT WOutputViewport : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WOutputViewport)
    Q_PROPERTY(WOutput* output READ output WRITE setOutput REQUIRED)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
    QML_NAMED_ELEMENT(OutputViewport)

public:
    explicit WOutputViewport(QQuickItem *parent = nullptr);
    ~WOutputViewport();

    WOutput *output() const;
    void setOutput(WOutput *newOutput);

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal newDevicePixelRatio);

Q_SIGNALS:
    void devicePixelRatioChanged();

private:
    void componentComplete() override;
};

WAYLIB_SERVER_END_NAMESPACE

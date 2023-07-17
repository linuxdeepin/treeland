// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WQuickSeat;
class WOutputViewportPrivate;
class WAYLIB_SERVER_EXPORT WOutputViewport : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WOutputViewport)
    Q_PROPERTY(WOutput* output READ output WRITE setOutput REQUIRED)
    Q_PROPERTY(WQuickSeat* seat READ seat WRITE setSeat NOTIFY seatChanged)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
    QML_NAMED_ELEMENT(OutputViewport)

public:
    explicit WOutputViewport(QQuickItem *parent = nullptr);
    ~WOutputViewport();

    static WOutputViewport *get(WOutput *output);

    WOutput *output() const;
    void setOutput(WOutput *newOutput);

    WQuickSeat *seat() const;
    void setSeat(WQuickSeat *newSeat);

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal newDevicePixelRatio);

Q_SIGNALS:
    void seatChanged();
    void devicePixelRatioChanged();
    void frameDone();

private:
    void classBegin() override;
    void componentComplete() override;
    void releaseResources() override;
};

WAYLIB_SERVER_END_NAMESPACE

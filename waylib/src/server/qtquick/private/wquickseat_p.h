// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wquickwaylandserver.h>

Q_MOC_INCLUDE(<wseat.h>)
Q_MOC_INCLUDE(<wquickcursor.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WSeatEventFilter;
class WQuickCursor;
class WQuickSeatPrivate;
class WAYLIB_SERVER_EXPORT WQuickSeat : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickSeat)
    QML_NAMED_ELEMENT(Seat)
    Q_PROPERTY(WSeat* seat READ seat NOTIFY seatChanged)
    Q_PROPERTY(QString name READ name WRITE setName REQUIRED)
    Q_PROPERTY(WQuickCursor* cursor READ cursor WRITE setCursor NOTIFY cursorChanged)
    Q_PROPERTY(WSeatEventFilter* eventFilter READ eventFilter WRITE setEventFilter NOTIFY eventFilterChanged)

public:
    explicit WQuickSeat(QObject *parent = nullptr);

    QString name() const;
    void setName(const QString &newName);

    WQuickCursor *cursor() const;
    void setCursor(WQuickCursor *cursor);

    WSeat *seat() const;

    WSeatEventFilter *eventFilter() const;
    void setEventFilter(WSeatEventFilter *newEventFilter);

public Q_SLOTS:
    void addDevice(WInputDevice *device);

Q_SIGNALS:
    void seatChanged();
    void cursorChanged();
    void eventFilterChanged();

private:
    friend class WOutputViewport;
    void addOutput(WOutput *output);
    void removeOutput(WOutput *output);

    void create() override;
    void polish() override;
};

WAYLIB_SERVER_END_NAMESPACE

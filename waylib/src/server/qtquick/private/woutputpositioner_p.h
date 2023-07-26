// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputpositioner.h"

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputPositionerAttached : public QObject
{
    friend class WOutputPositioner;
    Q_OBJECT
    Q_PROPERTY(WOutputPositioner* positioner READ positioner NOTIFY positionerChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WOutputPositionerAttached(QObject *parent = nullptr);

    WOutputPositioner *positioner() const;

Q_SIGNALS:
    void positionerChanged();

private:
    void setPositioner(WOutputPositioner *positioner);

private:
    WOutputPositioner *m_positioner = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE

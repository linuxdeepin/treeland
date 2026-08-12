// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>
#include <WServer>
#include <limits.h>

Q_MOC_INCLUDE("woutput.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputLayoutPrivate;
class WAYLIB_SERVER_EXPORT WOutputLayout : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutputLayout)
    Q_PROPERTY(int implicitWidth READ implicitWidth NOTIFY implicitWidthChanged)
    Q_PROPERTY(int implicitHeight READ implicitHeight NOTIFY implicitHeightChanged)

public:
    enum class Layer {
        Background = -999,
        Cursor = INT_MAX-1000
        // Ensure the Cursor's z-axis is above all other components
        // DefaultWindowDecoration = 1000000, at least it must be greater than 1000000
        // QQuickItem's z is qreal value, INT_MAX is not the maximum value but big enough
    };
    Q_ENUM(Layer)

    explicit WOutputLayout(WServer *server);

    wlr_output_layout *handle() const;

    const QList<WOutput *> &outputs() const;

    void add(WOutput *output, const QPoint &pos);
    void autoAdd(WOutput *output);
    void move(WOutput *output, const QPoint &pos);
    void remove(WOutput *output);

    QList<WOutput*> getIntersectedOutputs(const QRect &geometry) const;

    int implicitWidth() const;
    int implicitHeight() const;

Q_SIGNALS:
    void outputAdded(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void outputRemoved(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void outputsChanged();
    void implicitWidthChanged();
    void implicitHeightChanged();

protected:
    WOutputLayout(WOutputLayoutPrivate &dd, WServer *server);
};

WAYLIB_SERVER_END_NAMESPACE

// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <qwglobal.h>

#include <QQmlEngine>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWBackend;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WInputDevice;
class WQuickBackendPrivate;
class WAYLIB_SERVER_EXPORT WQuickBackend : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickBackend)
    Q_PROPERTY(WSocket* ownsSocket READ ownsSocket CONSTANT)
    QML_NAMED_ELEMENT(WaylandBackend)

public:
    explicit WQuickBackend(QObject *parent = nullptr);

    QW_NAMESPACE::QWBackend *backend() const;

Q_SIGNALS:
    void outputAdded(WOutput *output);
    void outputRemoved(WOutput *output);

    void inputAdded(WInputDevice *input);
    void inputRemoved(WInputDevice *input);

private:
    void create() override;
    void polish() override;
};

WAYLIB_SERVER_END_NAMESPACE

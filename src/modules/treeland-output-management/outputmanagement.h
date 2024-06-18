// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutput.h>
#include <wquickwaylandserver.h>

#include <qwsignalconnector.h>

#include <QList>
#include <QQmlEngine>

struct treeland_output_manager_v1;
WAYLIB_SERVER_USE_NAMESPACE

class TreelandOutputManager : public Waylib::Server::WQuickWaylandServerInterface
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(const char *primaryOutput READ primaryOutput WRITE setPrimaryOutput NOTIFY primaryOutputChanged)

public:
    explicit TreelandOutputManager(QObject *parent = nullptr);

    const char *primaryOutput();
    bool setPrimaryOutput(const char *name);
    Q_INVOKABLE void newOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    Q_INVOKABLE void removeOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output);

protected:
    WServerInterface *create() override;

Q_SIGNALS:
    void requestSetPrimaryOutput(const char *);
    void primaryOutputChanged();

private:
    treeland_output_manager_v1 *m_handle{ nullptr };
    QList<WAYLIB_SERVER_NAMESPACE::WOutput *> m_outputs;
};

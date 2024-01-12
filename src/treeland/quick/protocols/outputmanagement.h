// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwsignalconnector.h>
#include <wquickwaylandserver.h>

#include <QList>
#include <QQmlEngine>

struct treeland_output_manager_v1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

class TreelandOutputManager : public Waylib::Server::WQuickWaylandServerInterface {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(const char *primaryOutput READ primaryOutput WRITE setPrimaryOutput NOTIFY primaryOutputChanged)

public:
    explicit TreelandOutputManager(QObject *parent = nullptr);

    const char *primaryOutput();
    bool setPrimaryOutput(const char *name);
    Q_INVOKABLE void newOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    Q_INVOKABLE void removeOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output);

public Q_SLOTS:
    void on_set_primary_output(void *data);

protected:
    void create() override;

Q_SIGNALS:
    void requestSetPrimaryOutput(const char *);
    void primaryOutputChanged();

private:
    treeland_output_manager_v1 *m_handle { nullptr };
    QList<WAYLIB_SERVER_NAMESPACE::WOutput*> m_outputs;
    QW_NAMESPACE::QWSignalConnector m_sc;
};

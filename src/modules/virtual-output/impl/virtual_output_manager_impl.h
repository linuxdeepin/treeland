// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "server-protocol.h"
#include <wayland-server-core.h>
#include <qwsignalconnector.h>
#include <qwdisplay.h>
#include <QObject>

#include <QList>
#include <QString>

struct treeland_virtual_output_v1;

struct treeland_virtual_output_manager_v1 : public QObject
{
    Q_OBJECT
public:
    ~treeland_virtual_output_manager_v1();

    wl_global *global{ nullptr };
    wl_list resources;
    wl_event_loop *event_loop{ nullptr };
    QList<treeland_virtual_output_v1 *> virtual_output;

    static treeland_virtual_output_manager_v1 *create(QW_NAMESPACE::QWDisplay *display);

Q_SIGNALS:
    void virtualOutputCreated(treeland_virtual_output_v1 *virtual_output);
    void virtualOutputDestroy(treeland_virtual_output_v1 *virtual_output);
    void beforeDestroy();
};

struct treeland_virtual_output_v1 : public QObject
{
    Q_OBJECT
public:
    ~treeland_virtual_output_v1();
    treeland_virtual_output_manager_v1 *manager{ nullptr };
    wl_resource *resource{ nullptr };
    QString name; //Client-defined group name
    struct wl_array *screen_outputs;

    QStringList outputList;

    void send_outputs(QString name, struct wl_array *outputs);
    void send_error(uint32_t code, const char *message);  // tode: send err code and message

Q_SIGNALS:
    void beforeDestroy();
};

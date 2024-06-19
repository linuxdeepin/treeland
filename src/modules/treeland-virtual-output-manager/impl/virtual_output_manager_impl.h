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

#include <vector>
struct treeland_virtual_output_v1;

//treeland_virtual_output_manager_v1_interface
//treeland_virtual_output_v1_interface

struct treeland_virtual_output_manager_v1 : public QObject
{
    Q_OBJECT
public:
    // treeland_virtual_output_manager_v1 *manager{ nullptr };
    ~treeland_virtual_output_manager_v1();

    wl_global *global{ nullptr };
    wl_list resources;
    wl_event_loop *event_loop{ nullptr };
    QList<treeland_virtual_output_v1 *> virtual_output;

    static treeland_virtual_output_manager_v1 *create(QW_NAMESPACE::QWDisplay *display);

Q_SIGNALS:
    void virtualOutputCreated(treeland_virtual_output_v1 *virtual_output);
    void beforeDestroy();
};

struct treeland_virtual_output_v1 : public QObject
{
    Q_OBJECT
public:
    ~treeland_virtual_output_v1();
    treeland_virtual_output_manager_v1 *manager{ nullptr };
    wl_resource *resource{ nullptr };
    QString name;

    void send_outputs(struct wl_array *outputs);
    void send_error(uint32_t code, const char *message);

Q_SIGNALS:
    void beforeDestroy();
    // void backgroundTypeChanged(treeland_virtual_output_v1 *handle);
};

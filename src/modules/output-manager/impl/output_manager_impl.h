// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "treeland-output-management-protocol.h"

#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwsignalconnector.h>

#include <QObject>

#define TREELAND_OUTPUT_MANAGER_V1_VERSION 1

struct treeland_output_manager_v1 : public QObject
{
    Q_OBJECT
public:
    ~treeland_output_manager_v1();
    wl_global *global;
    wl_list resources;
    const char *primary_output_name{ nullptr };

    static treeland_output_manager_v1 *create(QW_NAMESPACE::qw_display *display);
    void set_primary_output(const char *name);

Q_SIGNALS:
    void requestSetPrimaryOutput(const char *name);
    void colorControlCreated(struct treeland_output_color_control_v1 *control);
    void before_destroy();
};

struct treeland_output_color_control_v1 : public QObject
{
    Q_OBJECT
public:
    ~treeland_output_color_control_v1();

    static treeland_output_color_control_v1 *from_resource(struct wl_resource *resource);

    treeland_output_manager_v1 *manager;
    wl_resource *resource;
    qw_output *output;

    void sendColorTemperature(uint32_t temperature);
    void sendBrightness(qreal brightness);

Q_SIGNALS:
    void requestSetColorTemperature(uint32_t temperature);
    void requestSetBrightness(qreal brightness);
    void before_destroy();
};

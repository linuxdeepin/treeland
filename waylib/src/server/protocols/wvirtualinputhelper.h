// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <QObject>

QW_BEGIN_NAMESPACE
class qw_input_device;
QW_END_NAMESPACE

struct wlr_output;
struct wlr_seat;
struct wlr_virtual_keyboard_v1;
struct wlr_virtual_pointer_v1_new_pointer_event;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WInputDevice;
class WSeat;
class WServer;
class WVirtualInputHelperPrivate;
class WAYLIB_SERVER_EXPORT WVirtualInputHelper : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WVirtualInputHelper)

public:
    explicit WVirtualInputHelper(WServer *server, WSeat *seat);
    ~WVirtualInputHelper() override;

private:
    void handleNewVKV1(::wlr_virtual_keyboard_v1 *virtualKeyboard);
    void handleNewVPV1(::wlr_virtual_pointer_v1_new_pointer_event *event);
    bool shouldAcceptSeat(::wlr_seat *suggestedSeat) const;
    WInputDevice *ensureDevice(QW_NAMESPACE::qw_input_device *handle) const;
    void attachDevice(WInputDevice *device);
    void maybeMapToOutput(WInputDevice *device, ::wlr_output *output) const;
};

WAYLIB_SERVER_END_NAMESPACE

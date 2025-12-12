// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QKeySequence>

#include "qwglobal.h"

#include "wglobal.h"

QW_BEGIN_NAMESPACE
class qw_display;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSocket;
WAYLIB_SERVER_END_NAMESPACE

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

struct wl_global;
struct wl_resource;

class treeland_shortcut_manager_v2 : public QObject
{
    Q_OBJECT
public:
    ~treeland_shortcut_manager_v2();
    static treeland_shortcut_manager_v2* create(qw_display *display);

    void sendActivated(WSocket *sessionSocket, const QString& name, bool repeat);
    void sendCommitSuccess(WSocket *sessionSocket);
    void sendCommitFailure(WSocket *sessionSocket, const QString& name, uint error);
    void sendInvalidCommit(WSocket *sessionSocket);

    wl_global *global = nullptr;

    QMap<WSocket*, wl_resource*> ownerClients;

Q_SIGNALS:
    void requestUnregisterShortcut(WSocket* sessionSocket, const QString& name);
    void requestBindKey(WSocket* sessionSocket,
                        const QString& name,
                        const QString& key,
                        uint mode,
                        uint action);
    void requestBindSwipeGesture(WSocket* sessionSocket, const QString& name, uint finger, uint direction, uint action);
    void requestBindHoldGesture(WSocket* sessionSocket, const QString& name, uint finger, uint action);
    void requestCommit(WSocket* sessionSocket);
    void before_destroy();
};

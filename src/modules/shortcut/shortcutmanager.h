// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "shortcutcontroller.h"

#include <wserver.h>

#include <QInputEvent>
#include <QObject>
#include <QQmlEngine>

class ShortcutManagerV2Private;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WSeat;
class WSocket;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class ShortcutManagerV2
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT

public:
    explicit ShortcutManagerV2(QObject *parent = nullptr);
    ~ShortcutManagerV2() override;
    QByteArrayView interfaceName() const override;
    static constexpr int InterfaceVersion = 2;

    ShortcutController* controller();
    void sendActivated(const QString& name, ShortcutController::KeyFlags keyFlags);

    bool tryHandleCaptureEvent(WAYLIB_SERVER_NAMESPACE::WSeat *seat, QInputEvent *event);
    bool isCaptureActive();

public Q_SLOTS:
    void onSessionChanged();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

Q_SIGNALS:
    void before_destroy();

private:
    std::unique_ptr<ShortcutManagerV2Private> d;
};

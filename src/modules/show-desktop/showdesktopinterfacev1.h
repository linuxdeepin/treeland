// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QQmlEngine>

WAYLIB_SERVER_USE_NAMESPACE

class ShowDesktopInterfaceV1Private;
class ShowDesktopInterfaceV1
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
    Q_PROPERTY(State desktopState READ desktopState WRITE setDesktopState NOTIFY desktopStateChanged)

public:
    enum class State
    {
        Normal,
        Show,
    };
    Q_ENUM(State)

    explicit ShowDesktopInterfaceV1(QObject *parent = nullptr);
    ~ShowDesktopInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;
    State desktopState();
    void setDesktopState(State state);

Q_SIGNALS:
    void desktopStateChanged();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<ShowDesktopInterfaceV1Private> d;
};

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QQmlEngine>

WAYLIB_SERVER_USE_NAMESPACE

class WindowManagementInterfaceV1Private;
class WindowManagementInterfaceV1
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
    Q_PROPERTY(DesktopState desktopState READ desktopState WRITE setDesktopState NOTIFY desktopStateChanged)

public:
    enum class DesktopState
    {
        Normal,
        Show,
        Preview
    };
    Q_ENUM(DesktopState)

    explicit WindowManagementInterfaceV1(QObject *parent = nullptr);
    ~WindowManagementInterfaceV1() override;

    static constexpr int InterfaceVersion = 1;
    DesktopState desktopState();
    void setDesktopState(DesktopState state);

Q_SIGNALS:
    void desktopStateChanged();
    void requestShowDesktop(uint32_t state);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<WindowManagementInterfaceV1Private> d;
};

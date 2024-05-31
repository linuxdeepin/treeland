// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wquickwaylandserver.h>

#include <QQmlEngine>

struct treeland_window_management_v1;
WAYLIB_SERVER_USE_NAMESPACE

class TreelandWindowManagement : public Waylib::Server::WQuickWaylandServerInterface
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(DesktopState desktopState READ desktopState WRITE setDesktopState NOTIFY desktopStateChanged)

public:
    enum class DesktopState { Normal, Show, Preview };
    Q_ENUM(DesktopState)

    explicit TreelandWindowManagement(QObject *parent = nullptr);

    DesktopState desktopState();
    void setDesktopState(DesktopState state);

protected:
    WServerInterface *create() override;

Q_SIGNALS:
    void requestShowDesktop(uint32_t state);
    void desktopStateChanged();

private:
    treeland_window_management_v1 *m_handle{ nullptr };
};

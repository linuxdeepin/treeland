// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "common/shellaction.h"

#include <wserver.h>

#include <QObject>
#include <QQmlEngine>

#include <optional>

WAYLIB_SERVER_USE_NAMESPACE

class CompositorActionInterfaceV1Private;

/**
 * @brief Server implementation of treeland_compositor_action_unstable_v1.
 *
 * Privileged global that lets a DDE shell client (dock, shutdown applet,
 * greeter, ...) trigger window-independent compositor actions one-shot,
 * such as workspace switching, show-desktop, the multitask overview,
 * lockscreen and the shutdown menu.
 */
class CompositorActionInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit CompositorActionInterfaceV1(QObject *parent = nullptr);
    ~CompositorActionInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

    // Maps a treeland-compositor-action-v1 protocol action value (see the
    // action enum of the generated QtWaylandServer type) onto a ShellAction.
    [[nodiscard]] static std::optional<ShellAction> mapCompositorAction(uint32_t protocolAction);

Q_SIGNALS:
    void triggered(uint32_t action);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<CompositorActionInterfaceV1Private> d;
};

// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>
#include <QQmlEngine>

WAYLIB_SERVER_USE_NAMESPACE

/**
 * @brief Server implementation of treeland_compositor_action_unstable_v1.
 *
 * Privileged global that lets a DDE shell client (dock, shutdown applet,
 * greeter, ...) trigger window-independent compositor actions one-shot,
 * such as workspace switching, show-desktop, the multitask overview,
 * lockscreen and the shutdown menu.
 *
 * The interface is stateless and fire-and-forget: each trigger() request is
 * an independent action and no events are sent back to the client. Binding
 * is restricted to compositor-authorized DDE clients via the global filter
 * set up by Helper (see Helper::init); unknown or currently unavailable
 * actions are ignored instead of disconnecting the client.
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

Q_SIGNALS:
    /**
     * @brief Emitted for every trigger() request with the raw protocol
     * action value (see the action enum of
     * treeland_compositor_action_unstable_v1). The compositor routes it
     * to the matching internal implementation.
     */
    void triggered(uint32_t action);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

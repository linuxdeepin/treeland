// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>
#include <QRectF>
#include <memory>
#include <optional>

class SurfaceWrapper;
class WindowAnimationManagerInterfaceV1Private;

/**
 * Server-side implementation of treeland_window_animation_v1.
 *
 * Each rect object is persistent: the client may commit multiple times to
 * update the geometry. The rect is associated with an xdg_activation_token_v1
 * resource; at token-commit time the compositor retrieves the local rect
 * (via takeCommittedRect), and at activation time it associates the rect
 * object with the target window's wrapper (via associatePendingRect) for
 * both open and close animations.
 */
class WindowAnimationManagerInterfaceV1
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit WindowAnimationManagerInterfaceV1(QObject *parent = nullptr);
    ~WindowAnimationManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

    /**
     * Called by the activation manager when a token is about to be committed.
     * If a window animation rect has been committed for this
     * xdg_activation_token_v1 resource, returns its local geometry and
     * stores the rect object internally for later association. Returns
     * std::nullopt otherwise.
     */
    std::optional<QRectF> takeCommittedRect(wl_resource *tokenResource);

    /**
     * Associates the most recently taken rect object (from takeCommittedRect)
     * with the target wrapper and the originating wrapper. Must be called
     * synchronously after takeCommittedRect returned a value. Returns true
     * if the rect was still alive and was associated, false if it was
     * destroyed in the meantime.
     */
    bool associatePendingRect(SurfaceWrapper *targetWrapper, SurfaceWrapper *originWrapper);

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    friend class WindowAnimationRectV1;
    std::unique_ptr<WindowAnimationManagerInterfaceV1Private> d;
};

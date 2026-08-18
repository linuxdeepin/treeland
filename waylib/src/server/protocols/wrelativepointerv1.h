// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>

#include <QObject>

#include <cstdint>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WRelativePointerManagerV1Private;

// Wayland `zwp_relative_pointer_manager_v1` wrapper. Created alongside
// WPointerConstraintsV1 because a locked pointer must receive relative motion
// deltas (via zwp_relative_pointer_v1) instead of absolute coordinates.
// WCursorPrivate::applyPointerConstraint calls sendRelativeMotion() on every
// pointer motion while a locked constraint is active.
class WAYLIB_SERVER_EXPORT WRelativePointerManagerV1
    : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WRelativePointerManagerV1)

public:
    explicit WRelativePointerManagerV1();

    wlr_relative_pointer_manager_v1 *handle() const;

    QByteArrayView interfaceName() const override;

    // Deliver a relative motion event to the seat. `timeUsec` is in
    // microseconds (unlike wl_pointer.motion which uses milliseconds).
    void sendRelativeMotion(wlr_seat *seat, uint64_t timeUsec,
                            double dx, double dy,
                            double dxUnaccel, double dyUnaccel) const;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

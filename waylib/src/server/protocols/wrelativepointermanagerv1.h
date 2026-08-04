// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>

#include <QObject>
#include <QPointF>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;

class WRelativePointerManagerV1Private;
class WAYLIB_SERVER_EXPORT WRelativePointerManagerV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WRelativePointerManagerV1)

public:
    explicit WRelativePointerManagerV1();

    wlr_relative_pointer_manager_v1 *handle() const;

    void sendRelativeMotion(WSeat *seat, uint32_t timeMsec, const QPointF &delta,
                            const QPointF &unacceleratedDelta);

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

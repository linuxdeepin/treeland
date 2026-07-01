// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

#include <QObject>
#include <QPointF>

QW_BEGIN_NAMESPACE
class qw_relative_pointer_manager_v1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;

class WAYLIB_SERVER_EXPORT WRelativePointerManagerV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT

public:
    explicit WRelativePointerManagerV1();

    QW_NAMESPACE::qw_relative_pointer_manager_v1 *handle() const;

    void sendRelativeMotion(WSeat *seat, uint32_t timeMsec, const QPointF &delta,
                            const QPointF &unacceleratedDelta);

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

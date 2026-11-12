// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>

struct wlr_pointer_gestures_v1;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WPointerGesturesV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    wlr_pointer_gestures_v1 *handle() const;
    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

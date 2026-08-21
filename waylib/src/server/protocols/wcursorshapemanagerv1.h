// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursor;

class WCursorShapeManagerV1Private;
class WAYLIB_SERVER_EXPORT WCursorShapeManagerV1 : public QObject, public WObject,  public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WCursorShapeManagerV1)

public:
    explicit WCursorShapeManagerV1();

    wlr_cursor_shape_manager_v1 *handle() const;

    QByteArrayView interfaceName() const override;
    static constexpr int InterfaceVersion = 2;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

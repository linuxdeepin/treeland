// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputLayout;
class WXdgOutputManagerPrivate;
class WAYLIB_SERVER_EXPORT WXdgOutputManager : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgOutputManager)

public:
    explicit WXdgOutputManager(WOutputLayout *layout);

    WOutputLayout *layout() const;

    void setScaleOverride(qreal scaleOverride);
    qreal scaleOverride() const;
    void resetScaleOverride();

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void scaleOverrideChanged();

protected:
    void create(WServer *wserver) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

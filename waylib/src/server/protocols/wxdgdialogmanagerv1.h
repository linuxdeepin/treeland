// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgToplevelSurface;
class WXdgDialogManagerV1Private;

class WAYLIB_SERVER_EXPORT WXdgDialogManagerV1
    : public QObject
    , public WObject
    , public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgDialogManagerV1)

public:
    explicit WXdgDialogManagerV1(QObject *parent = nullptr);
    ~WXdgDialogManagerV1() override;

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void surfaceModalChanged(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface, bool modal);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

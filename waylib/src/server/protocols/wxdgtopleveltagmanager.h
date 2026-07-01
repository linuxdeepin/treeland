// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class WXdgToplevelTagManagerV1Private;

class WAYLIB_SERVER_EXPORT WXdgToplevelTagManagerV1
    : public QObject
    , public WObject
    , public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgToplevelTagManagerV1)
    QML_NAMED_ELEMENT(XdgToplevelTagManagerV1)
    QML_UNCREATABLE("Can't create in qml")

public:
    explicit WXdgToplevelTagManagerV1();
    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *wserver) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE

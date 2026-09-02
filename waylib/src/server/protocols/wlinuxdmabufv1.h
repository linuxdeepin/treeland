// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WLinuxDmabufV1 : public QObject, public WServerInterface
{
    Q_OBJECT

public:
    explicit WLinuxDmabufV1(wlr_renderer *renderer, QObject *parent = nullptr);

    wlr_linux_dmabuf_v1 *handle() const;

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    wlr_renderer *m_renderer = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE

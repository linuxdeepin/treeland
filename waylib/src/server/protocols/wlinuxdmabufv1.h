// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

QW_BEGIN_NAMESPACE
class qw_linux_dmabuf_v1;
class qw_renderer;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WLinuxDmabufV1 : public QObject, public WServerInterface
{
    Q_OBJECT

public:
    explicit WLinuxDmabufV1(QW_NAMESPACE::qw_renderer *renderer, QObject *parent = nullptr);

    QW_NAMESPACE::qw_linux_dmabuf_v1 *handle() const;

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    QW_NAMESPACE::qw_renderer *m_renderer = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE

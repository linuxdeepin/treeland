// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

struct treeland_output_manager_v1;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class PrimaryOutputV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT

public:
    explicit PrimaryOutputV1(QObject *parent = nullptr);

    void sendPrimaryOutput(const char *name);
    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

Q_SIGNALS:
    void requestSetPrimaryOutput(const char *name);

private:
    treeland_output_manager_v1 *m_handle{ nullptr };
};

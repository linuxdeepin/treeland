// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

struct treeland_output_manager_v1;
struct treeland_output_color_control_v1;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class OutputManagerV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT

public:
    explicit OutputManagerV1(QObject *parent = nullptr);

    void sendPrimaryOutput(const char *name);
    QByteArrayView interfaceName() const override;

    void onColorControlCreated(treeland_output_color_control_v1 *control);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

Q_SIGNALS:
    void requestSetPrimaryOutput(const char *name);

private:
    treeland_output_manager_v1 *m_handle{ nullptr };
};

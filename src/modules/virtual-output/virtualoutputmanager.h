// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include "impl/virtual_output_manager_impl.h"

#include <wserver.h>

struct treeland_virtual_output_v1;
struct treeland_virtual_output_manager_v1;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class VirtualOutputV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT

public:
    explicit VirtualOutputV1(QObject *parent = nullptr);

    QByteArrayView interfaceName() const override;

    QList<treeland_virtual_output_v1 *> m_virtualOutputv1;

Q_SIGNALS:

    void requestCreateVirtualOutput(treeland_virtual_output_v1 *virtual_output);
    void destroyVirtualOutput(treeland_virtual_output_v1 *virtual_output);

private Q_SLOTS:
    void onVirtualOutputCreated(treeland_virtual_output_v1 *virtual_output);
    void onVirtualOutputDestroy(treeland_virtual_output_v1 *virtual_output);

private:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

    treeland_virtual_output_manager_v1 *m_manager{ nullptr };
};

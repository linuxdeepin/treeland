// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>
#include <wserver.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WVirtualKeyboardManagerV1Private;
class WAYLIB_SERVER_EXPORT WVirtualKeyboardManagerV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WVirtualKeyboardManagerV1)
public:
    explicit WVirtualKeyboardManagerV1(QObject *parent = nullptr);

    QByteArrayView interfaceName() const override;
    wlr_virtual_keyboard_manager_v1 *handle() const;

Q_SIGNALS:
    void newVirtualKeyboard(wlr_virtual_keyboard_v1 *virtualKeyboard);

private:
    void create(WServer *server) override;
    wl_global *global() const override;
};
WAYLIB_SERVER_END_NAMESPACE

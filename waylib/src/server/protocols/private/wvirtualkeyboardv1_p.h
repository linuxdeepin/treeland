// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wserver.h>

#include <qwglobal.h>

Q_MOC_INCLUDE(<qwvirtualkeyboardv1.h>)

QW_BEGIN_NAMESPACE
class qw_virtual_keyboard_v1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WVirtualKeyboardManagerV1Private;
class WAYLIB_SERVER_EXPORT WVirtualKeyboardManagerV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WVirtualKeyboardManagerV1)
public:
    explicit WVirtualKeyboardManagerV1(QObject *parent = nullptr);

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void newVirtualKeyboard(QW_NAMESPACE::qw_virtual_keyboard_v1 *virtualKeyboard);

private:
    void create(WServer *server) override;
    wl_global *global() const override;
};
WAYLIB_SERVER_END_NAMESPACE

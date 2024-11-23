// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "baseplugininterface.h"

#include <QObject>

class Output;
class QQuickItem;

class ILockScreen : public virtual BasePluginInterface
{
public:
    virtual ~ILockScreen() = default;

    virtual QQuickItem *createLockScreen(Output *output, QQuickItem *parent) = 0;
};

Q_DECLARE_INTERFACE(ILockScreen, "org.deepin.treeland.v1.LockScreenInterface")

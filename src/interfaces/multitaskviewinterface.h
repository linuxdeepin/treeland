// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "baseplugininterface.h"

#include <QObject>

class IMultitaskView : public virtual BasePluginInterface
{
public:
    virtual ~IMultitaskView() = default;

    enum ActiveReason
    {
        ShortcutKey = 1,
        Gesture
    };

    enum Status
    {
        Uninitialized,
        Initialized,
        Active,
        Exited
    };

    virtual void setStatus(IMultitaskView::Status status) = 0;
    virtual void toggleMultitaskView(IMultitaskView::ActiveReason reason) = 0;
};

Q_DECLARE_INTERFACE(IMultitaskView, "org.deepin.treeland.v1.MultitaskViewInterface")

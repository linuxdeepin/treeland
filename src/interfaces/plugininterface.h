// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "baseplugininterface.h"

#include <QObject>

class TreelandProxyInterface;

class PluginInterface : public virtual BasePluginInterface
{
public:
    virtual ~PluginInterface() { }

    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual void initialize(TreelandProxyInterface *proxy) = 0;
    virtual void shutdown() = 0;
    virtual bool enabled() const = 0;
};

Q_DECLARE_INTERFACE(PluginInterface, "org.deepin.treeland.v1.PluginInterface")

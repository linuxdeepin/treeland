// Copyright (C) 2024 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>

class Workspace;
class QmlEngine;

class RootSurfaceContainer;

class TreelandProxyInterface
{
public:
    virtual ~TreelandProxyInterface() { }

    virtual QmlEngine *qmlEngine() const = 0;
    virtual Workspace *workspace() const = 0;

    virtual RootSurfaceContainer *rootSurfaceContainer() const = 0;
};

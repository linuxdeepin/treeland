#pragma once

#include <QAction>
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

    virtual void blockActivateSurface(bool block) = 0;
    virtual bool isBlockActivateSurface() const = 0;
};

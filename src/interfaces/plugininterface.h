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

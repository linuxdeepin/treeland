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

    virtual void toggleMultitaskView(IMultitaskView::ActiveReason reason) = 0;
};

Q_DECLARE_INTERFACE(IMultitaskView, "org.deepin.treeland.v1.MultitaskViewInterface")

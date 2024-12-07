// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surfacecontainer.h"

#include <map>
#include <memory>

class Output;
class QTimer;
class ILockScreen;

class LockScreen : public SurfaceContainer
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    enum class CurrentMode
    {
        Lock = 1,
        Shutdown = 2,
        SwitchUser = 3
    };
    Q_ENUM(CurrentMode)

    explicit LockScreen(ILockScreen *impl, SurfaceContainer *parent);

    bool isLocked() const;
    void lock();
    void shutdown();
    void switchUser();

Q_SIGNALS:
    void unlock();

public Q_SLOTS:
    void onAnimationPlayed();
    void onAnimationPlayFinished();

public:
    void addOutput(Output *output) override;
    void removeOutput(Output *output) override;

private:
    ILockScreen *m_impl{ nullptr };
    std::map<Output *, std::unique_ptr<QQuickItem, void (*)(QQuickItem *)>> m_components;
    std::unique_ptr<QTimer> m_delayTimer;
};

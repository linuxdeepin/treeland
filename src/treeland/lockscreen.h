// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surfacecontainer.h"

#include <map>
#include <memory>

class Output;
class QTimer;

class LockScreen : public SurfaceContainer
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit LockScreen(SurfaceContainer *parent);

    bool isLocked() const;
    void lock();

Q_SIGNALS:
    void unlock();

public Q_SLOTS:
    void onAnimationPlayed();
    void onAnimationPlayFinished();

public:
    void addOutput(Output *output) override;
    void removeOutput(Output *output) override;

private:
    std::map<Output *, std::unique_ptr<QQuickItem, void (*)(QQuickItem *)>> m_components;
    std::unique_ptr<QTimer> m_delayTimer;
};

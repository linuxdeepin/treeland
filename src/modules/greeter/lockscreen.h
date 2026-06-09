// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surface/surfacecontainer.h"

#include <map>
#include <memory>

class GreeterProxy;
class QTimer;

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

    explicit LockScreen(SurfaceContainer *parent, GreeterProxy *greeterProxy);

    bool available() const;
    bool isLocked() const;
    void lock();
    void shutdown();
    void switchUser();
    void setPrimaryOutputName(const QString &primaryOutputName);

Q_SIGNALS:
    void unlock();

public Q_SLOTS:
    void onAnimationPlayed();
    void onAnimationPlayFinished();

public:
    void addOutput(Output *output) override;
    void removeOutput(Output *output) override;

private:
    GreeterProxy *m_greeterProxy{ nullptr };
    std::map<Output *, std::unique_ptr<QQuickItem, void (*)(QQuickItem *)>> m_components;
    std::unique_ptr<QTimer> m_delayTimer;
    QString m_primaryOutputName;
};

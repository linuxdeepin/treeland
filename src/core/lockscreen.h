// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surface/surfacecontainer.h"
#include "wglobal.h"
#include <map>
#include <memory>

class GreeterProxy;
class QTimer;
class ILockScreen;

#ifdef EXT_SESSION_LOCK_V1
WAYLIB_SERVER_BEGIN_NAMESPACE
class WSessionLock;
class WSessionLockSurface;
class WOutputItem;
WAYLIB_SERVER_END_NAMESPACE
Q_MOC_INCLUDE(<wsessionlock.h>)
Q_MOC_INCLUDE(<wsessionlocksurface.h>)
#endif

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

    explicit LockScreen(ILockScreen *impl, SurfaceContainer *parent, GreeterProxy *greeterProxy);

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
#ifdef EXT_SESSION_LOCK_V1
    void onExternalLock(WSessionLock *lock);

private:
    void onOutputGeometryChanged();
    void doRemoveLockSurface(WSessionLockSurface *surface);
    void createFallbackItem(WOutputItem *outputItem);

private Q_SLOTS:
    void onLockSurfaceAdded(WSessionLockSurface *surface);
    void onLockSurfaceRemoved(WSessionLockSurface *surface);
    void onExternalLockAbandoned();
    void onExternalUnlock();
#endif

public:
    void addOutput(Output *output) override;
    void removeOutput(Output *output) override;

private:
    ILockScreen *m_impl{ nullptr };
    GreeterProxy *m_greeterProxy{ nullptr };
    std::map<Output *, std::unique_ptr<QQuickItem, void (*)(QQuickItem *)>> m_components;
    std::unique_ptr<QTimer> m_delayTimer;
#ifdef EXT_SESSION_LOCK_V1
    std::map<WOutputItem *, std::unique_ptr<WSessionLockSurface, std::function<void(WSessionLockSurface*)>>> m_lockSurfaces;
    std::map<WOutputItem *, std::unique_ptr<QQuickItem, std::function<void(QQuickItem*)>>> m_fallbackItems;
    WSessionLock* m_sessionLock{ nullptr };
#endif
};

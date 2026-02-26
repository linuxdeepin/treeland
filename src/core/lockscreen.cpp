// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "lockscreen.h"

#include "interfaces/lockscreeninterface.h"
#include "output/output.h"
#include "seat/helper.h"
#include "utils/cmdline.h"
#include "common/treelandlogging.h"
#include "greeter/greeterproxy.h"

#ifdef EXT_SESSION_LOCK_V1
#include <wsessionlock.h>
#include <wsessionlocksurface.h>

#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"
#include "woutputitem.h"
#endif

WAYLIB_SERVER_USE_NAMESPACE

LockScreen::LockScreen(ILockScreen *impl, SurfaceContainer *parent, GreeterProxy *greeterProxy)
    : SurfaceContainer(parent)
    , m_impl(impl)
    , m_greeterProxy(greeterProxy)
    , m_delayTimer(std::make_unique<QTimer>(new QTimer))
{
    connect(m_delayTimer.get(), &QTimer::timeout, this, &LockScreen::unlock);

    m_delayTimer->setSingleShot(true);
    // Display desktop animation after lock screen animation with a delay of 300ms
    m_delayTimer->setInterval(300);
}

void LockScreen::lock()
{
    if (isVisible()) {
        return;
    }

    setVisible(true);

    if (!m_greeterProxy->isLocked())
        m_greeterProxy->lock();
}

void LockScreen::shutdown()
{
    // ext_session_lock_v1 does not support shutdown
    if (!m_impl) {
        qCWarning(treelandShell) << "Attempt to shutdown with no compatible lockscreen implementation!";
        emit unlock();
        return;
    }

    if (isVisible()) {
        return;
    }

    setVisible(true);

    if (!m_greeterProxy->showShutdownView())
        m_greeterProxy->setShowShutdownView(true);
}

void LockScreen::switchUser()
{
    // ext_session_lock_v1 does not support user switching
    if (!m_impl) {
        qCWarning(treelandShell) << "Attempt to switch user with no compatible lockscreen implementation!";
        emit unlock();
        return;
    }

    if (isVisible()) {
        return;
    }

    setVisible(true);

    Q_EMIT m_greeterProxy->switchUser();
}

void LockScreen::addOutput(Output *output)
{
    SurfaceContainer::addOutput(output);
#if EXT_SESSION_LOCK_V1
    auto outputItem = output->outputItem();
    connect(outputItem, &WOutputItem::geometryChanged, this, &LockScreen::onOutputGeometryChanged);

    const auto &[_, ok] = m_lockSurfaces.emplace(outputItem, nullptr);
    Q_ASSERT(ok);
#endif

    if (!m_impl) {
        return;
    }

    auto *item = m_impl->createLockScreen(output, this);

    connect(item, SIGNAL(animationPlayed()), this, SLOT(onAnimationPlayed()));
    connect(item, SIGNAL(animationPlayFinished()), this, SLOT(onAnimationPlayFinished()));

    m_components.insert(
        { output, std::unique_ptr<QQuickItem, void (*)(QQuickItem *)>(item, [](QQuickItem *item) {
              item->deleteLater();
          }) });
}

bool LockScreen::isLocked() const
{
    return isVisible();
}

void LockScreen::removeOutput(Output *output)
{
    SurfaceContainer::removeOutput(output);
#if EXT_SESSION_LOCK_V1
    auto outputItem = output->outputItem();
    m_lockSurfaces.erase(outputItem);
    m_fallbackItems.erase(outputItem);

    if (!m_impl) {
        return;
    }
#endif
    m_components.erase(output);
}

void LockScreen::onAnimationPlayed()
{
    if (!m_delayTimer->isActive()) {
        m_delayTimer->start();
    }
}

void LockScreen::onAnimationPlayFinished()
{
    auto *item = qobject_cast<QQuickItem *>(sender());
    Q_ASSERT(item);

    setVisible(false);
}

bool LockScreen::available() const
{
    return m_impl != nullptr;
}

void LockScreen::setPrimaryOutputName(const QString &primaryOutputName)
{
    for (const auto &[k, v] : m_components) {
        v->setProperty("primaryOutputName", primaryOutputName);
    }
}

#if EXT_SESSION_LOCK_V1
// ext_session_lock_v1 capabilities

void LockScreen::doRemoveLockSurface(WSessionLockSurface *surface)
{
    if (!surface) {
        return;
    }

    auto wrapper = rootContainer()->getSurface(surface);
    if (!wrapper) {
        qCWarning(treelandShell) << "Removing lock surface with no container attached";
        return;
    }
    rootContainer()->destroyForSurface(wrapper);
}


void LockScreen::onLockSurfaceAdded(WSessionLockSurface *surface)
{
    if (!m_sessionLock) {
        return;
    }
    auto output = Helper::instance()->getOutput(surface->output());

    if (!output) {
        qCWarning(treelandShell) << "Lock surface added with no output!";
        return;
    }

    auto outputItem = output->outputItem();

    m_fallbackItems.erase(outputItem);
    // protocol error if multiple surfaces for one output
    Q_ASSERT(!m_lockSurfaces[outputItem]);
    m_lockSurfaces[outputItem] = std::unique_ptr<WSessionLockSurface, std::function<void(WSessionLockSurface*)>>(surface, [this](WSessionLockSurface* s) {
        doRemoveLockSurface(s);
    });

    // Configure the lock surface to match the output's full size
    surface->configureSize(outputItem->size().toSize());

    auto wrapper = new SurfaceWrapper(
        Helper::instance()->qmlEngine(), surface, SurfaceWrapper::Type::LockScreen);

    wrapper->setNoDecoration(true);
    wrapper->setSkipMutiTaskView(true);
    wrapper->setSkipSwitcher(true);
    wrapper->setOwnsOutput(output);
    wrapper->setPosition(outputItem->position());

    addSurface(wrapper);

    wrapper->setHasInitializeContainer(true);

    connect(wrapper, &SurfaceWrapper::requestActive, this, [wrapper] {
        Helper::instance()->activateSurface(wrapper);
    });
}

void LockScreen::onLockSurfaceRemoved(WSessionLockSurface *surface)
{
    if (!m_sessionLock) {
        return;
    }
    auto output = Helper::instance()->getOutput(surface->output());
    if (!output) {
        qCWarning(treelandShell) << "removing lock surface with no output!";
        return;
    }

    auto outputItem = output->outputItem();

    if (m_lockSurfaces[outputItem]) {
        if (isLocked()) {
            // display a solid color for output
            createFallbackItem(outputItem);
        }
        m_lockSurfaces[outputItem].reset();
    }
}

void LockScreen::onOutputGeometryChanged()
{
    WOutputItem *outputItem = qobject_cast<WOutputItem *>(QObject::sender());
    Q_ASSERT(m_lockSurfaces.find(outputItem) != m_lockSurfaces.end());

    auto *lockSurface = m_lockSurfaces[outputItem].get();
    if (lockSurface) {
        // Resize the lock surface to match the new output size
        lockSurface->configureSize(outputItem->size().toSize());

        auto wrapper = rootContainer()->getSurface(lockSurface);
        if (wrapper) {
            wrapper->setPosition(outputItem->position());
        }
    }
}

void LockScreen::onExternalLock(WSessionLock *lock)
{
    if (isVisible()) {
        lock->finish();
        return;
    }

    m_sessionLock = lock;

    lock->safeConnect(&WSessionLock::surfaceAdded, this, &LockScreen::onLockSurfaceAdded);
    lock->safeConnect(&WSessionLock::surfaceRemoved, this, &LockScreen::onLockSurfaceRemoved);
    lock->safeConnect(&WSessionLock::unlocked, this, &LockScreen::onExternalUnlock);
    lock->safeConnect(&WSessionLock::canceled, this, &LockScreen::onExternalUnlock);
    lock->safeConnect(&WSessionLock::abandoned, this, &LockScreen::onExternalLockAbandoned);

    setVisible(true);
}

void LockScreen::onExternalUnlock() {
    if (!m_sessionLock || !isLocked()) {
        qCCritical(treelandShell) << "Session lock identified as unlocked but screen is not externally locked?";
        return;
    }
    setVisible(false);
    for (auto &[_, surface] : m_lockSurfaces) {
        surface.reset();
    }
    m_fallbackItems.clear();
    m_sessionLock = nullptr;
    emit unlock();
}

void LockScreen::onExternalLockAbandoned() {
    if (!m_impl || !m_sessionLock) {
        return;
    }
    if (!isLocked()) {
        qCCritical(treelandShell) << "Session lock identified as abandoned but screen is not locked?";
        return;
    }

    qCWarning(treelandShell) << "locking client likely died";

    m_sessionLock = nullptr;

    for (auto &[_, surface] : m_lockSurfaces) {
        surface.reset();
    }
    m_fallbackItems.clear();

    m_greeterProxy->lock();
}

void LockScreen::createFallbackItem(WOutputItem *outputItem)
{
    if (!outputItem || m_fallbackItems.find(outputItem) != m_fallbackItems.end()) {
        return;
    }

    auto *engine = Helper::instance()->qmlEngine();
    Q_ASSERT(engine);

    auto *fallbackItem = engine->createLockScreenFallback(this, {
        {"outputItem", QVariant::fromValue(outputItem)}
    });
    Q_ASSERT(fallbackItem);

    m_fallbackItems[outputItem] = std::unique_ptr<QQuickItem, std::function<void(QQuickItem*)>>(
        fallbackItem, [](QQuickItem *item) {
            if (item) {
                item->deleteLater();
            }
        });
}
#endif

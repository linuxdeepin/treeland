// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "lockscreen.h"

#include "common/treelandlogging.h"
#include "core/qmlengine.h"
#include "modules/greeter/greeterproxy.h"
#include "output/output.h"
#include "seat/helper.h"

#include <QTimer>

LockScreen::LockScreen(SurfaceContainer *parent, GreeterProxy *greeterProxy)
    : SurfaceContainer(parent)
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
    if (isVisible())
        return;

    m_greeterProxy->updatePowerCapabilities();
    setVisible(true);

    if (!m_greeterProxy->isLocked())
        m_greeterProxy->lock();
}

void LockScreen::shutdown()
{
    if (isVisible())
        return;

    m_greeterProxy->updatePowerCapabilities();
    setVisible(true);

    if (!m_greeterProxy->showShutdownView())
        m_greeterProxy->setShowShutdownView(true);
}

void LockScreen::switchUser()
{
    if (isVisible())
        return;

    setVisible(true);
    Q_EMIT m_greeterProxy->switchUser();
}

void LockScreen::addOutput(Output *output)
{
    qCDebug(treelandShell) << "Adding output to lock screen:" << output;

    SurfaceContainer::addOutput(output);

    auto *item = Helper::instance()->qmlEngine()->createLockScreen(output, this);
    item->setProperty("primaryOutputName", m_primaryOutputName);

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
    qCDebug(treelandShell) << "Removing output from lock screen:" << output;

    SurfaceContainer::removeOutput(output);
    m_components.erase(output);
}

void LockScreen::onAnimationPlayed()
{
    if (!m_delayTimer->isActive())
        m_delayTimer->start();
}

void LockScreen::onAnimationPlayFinished()
{
    auto *item = qobject_cast<QQuickItem *>(sender());
    Q_ASSERT(item);

    setVisible(false);
}

bool LockScreen::available() const
{
    return true;
}

void LockScreen::setPrimaryOutputName(const QString &primaryOutputName)
{
    qCDebug(treelandShell) << "Setting primary output name for lock screen:" << primaryOutputName;

    m_primaryOutputName = primaryOutputName;
    for (const auto &[_, item] : std::as_const(m_components))
        item->setProperty("primaryOutputName", primaryOutputName);
}

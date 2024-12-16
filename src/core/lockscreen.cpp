// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "lockscreen.h"

#include "interfaces/lockscreeninterface.h"
#include "output/output.h"
#include "seat/helper.h"
#include "utils/cmdline.h"

LockScreen::LockScreen(ILockScreen *impl, SurfaceContainer *parent)
    : SurfaceContainer(parent)
    , m_impl(impl)
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

    for (const auto &[k, v] : m_components) {
        v->setProperty("currentMode", static_cast<int>(LockScreen::CurrentMode::Lock));
        QMetaObject::invokeMethod(v.get(), "start");
    }
}

void LockScreen::shutdown()
{
    if (isVisible()) {
        return;
    }

    setVisible(true);

    for (const auto &[k, v] : m_components) {
        v->setProperty("currentMode", static_cast<int>(LockScreen::CurrentMode::Shutdown));
        QMetaObject::invokeMethod(v.get(), "start");
    }
}

void LockScreen::switchUser()
{
    if (isVisible()) {
        return;
    }

    setVisible(true);

    for (const auto &[k, v] : m_components) {
        v->setProperty("currentMode", static_cast<int>(LockScreen::CurrentMode::SwitchUser));
        QMetaObject::invokeMethod(v.get(), "start");
    }
}

void LockScreen::addOutput(Output *output)
{
    SurfaceContainer::addOutput(output);

    auto engine = Helper::instance()->qmlEngine();
    auto *item = m_impl->createLockScreen(output, this);

    if (isVisible()) {
        QMetaObject::invokeMethod(item, "start");
    }

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

void LockScreen::setPrimaryOutputName(const QString &primaryOutputName)
{
    for (const auto &[k, v] : m_components) {
        v->setProperty("primaryOutputName", primaryOutputName);
    }
}

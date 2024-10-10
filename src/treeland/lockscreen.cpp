// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "lockscreen.h"

#include "helper.h"
#include "output.h"

// FIXME: greeter should support auth self.
bool IS_ENABLED = false;

LockScreen::LockScreen(SurfaceContainer *parent)
    : SurfaceContainer(parent)
    , m_delayTimer(std::make_unique<QTimer>(new QTimer))
{
    connect(m_delayTimer.get(), &QTimer::timeout, this, &LockScreen::unlock);

    m_delayTimer->setSingleShot(true);
    m_delayTimer->setInterval(300);

    // TODO: Use CommandLineOptions class to parse command line arguments
    QCommandLineOption socket({ "s", "socket" }, "set ddm socket", "socket");
    QCommandLineParser parser;
    parser.addOptions({ socket });
    parser.process(*qApp);
    IS_ENABLED = parser.isSet(socket);
}

void LockScreen::addOutput(Output *output)
{
    if (!IS_ENABLED) {
        return;
    }

    if (m_components.contains(output)) {
        return;
    }

    auto engine = Helper::instance()->qmlEngine();
    auto *item = engine->createLockScreen(output, this);
    item->forceActiveFocus();

    connect(item, SIGNAL(animationPlayed()), this, SLOT(onAnimationPlayed()));
    connect(item, SIGNAL(animationPlayFinished()), this, SLOT(onAnimationPlayFinished()));

    m_components.insert(
        { output, std::unique_ptr<QQuickItem, void (*)(QQuickItem *)>(item, [](QQuickItem *item) {
              item->deleteLater();
          }) });
}

void LockScreen::removeOutput(Output *output)
{
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
    item->setVisible(false);

    m_components.clear();
}

// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwpersonalizationmanager.h"

#include "personalization-server-protocol.h"
#include "personalizationmanager.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>
#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

#include <QDebug>
#include <QTimer>

#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
}

static QuickPersonalizationManager *PERSONALIZATION_MANAGER = nullptr;

class QuickPersonalizationManagerPrivate : public WObjectPrivate {
public:
    QuickPersonalizationManagerPrivate(QuickPersonalizationManager *qq)
        : WObjectPrivate(qq) {}
    ~QuickPersonalizationManagerPrivate() = default;

    W_DECLARE_PUBLIC(QuickPersonalizationManager)

    TreeLandPersonalizationManager *manager = nullptr;
};

QuickPersonalizationManager::QuickPersonalizationManager(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new QuickPersonalizationManagerPrivate(this), nullptr)
{
    if (PERSONALIZATION_MANAGER) {
        qFatal("There are multiple instances of QuickPersonalizationManager");
    }

    PERSONALIZATION_MANAGER = this;
}

void QuickPersonalizationManager::create() {
    W_D(QuickPersonalizationManager);
    WQuickWaylandServerInterface::create();

    d->manager = TreeLandPersonalizationManager::create(server()->handle());
    connect(d->manager,
            &TreeLandPersonalizationManager::windowContextCreated,
            this,
            &QuickPersonalizationManager::onWindowContextCreated);
}

void QuickPersonalizationManager::onWindowContextCreated(PersonalizationWindowContext *context)
{
    connect(context,
            &PersonalizationWindowContext::backgroundTypeChanged,
            this,
            &QuickPersonalizationManager::onBackgroundTypeChanged);
}

void QuickPersonalizationManager::onBackgroundTypeChanged(PersonalizationWindowContext *context)
{
    auto p = context->handle();
    if (!p)
        return;

    Q_EMIT backgroundTypeChanged(WSurface::fromHandle(p->surface), p->background_type);
}

QuickPersonalizationManagerAttached::QuickPersonalizationManagerAttached(WSurface *target, QuickPersonalizationManager *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    connect(m_manager, &QuickPersonalizationManager::backgroundTypeChanged, this, [this] (WSurface *surface, uint32_t type) {
        if (m_target == surface) {
            m_backgroundType = static_cast<BackGroundType>(type);
            Q_EMIT backgroundWallpaperChanged();
        }
    });
}

QuickPersonalizationManagerAttached *QuickPersonalizationManager::qmlAttachedProperties(QObject *target)
{
    if (auto *surface = qobject_cast<WXdgSurface*>(target)) {
        return new QuickPersonalizationManagerAttached(surface->surface(), PERSONALIZATION_MANAGER);
    }

    return nullptr;
}

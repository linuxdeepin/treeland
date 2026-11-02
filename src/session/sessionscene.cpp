// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "sessionscene.h"
#include "session.h"
#include "surface/surfacewrapper.h"
#include <utility>

SessionScene::SessionScene(Session *owner)
    : SessionComponent(owner), m_system(owner->username() == QStringLiteral("dde"))
{
    initializeActiveState();
}

SessionScene::~SessionScene()
{
    for (const auto &surface : std::as_const(m_surfaces)) {
        if (surface)
            surface->setRenderingEnabled(false);
    }
}

bool SessionScene::allows(SurfaceWrapper *surface) const
{
    // The dedicated login service is a system domain, not a logged-in user's
    // desktop. A privileged role on an ordinary user's window grants no bypass.
    const bool systemSurface = m_system
        && (surface->type() == SurfaceWrapper::Type::LockScreen
            || surface->surfaceRole() == SurfaceWrapper::SurfaceRole::PrivilegedOverlay);
    return !surface->isAboutToRemove() && (isActive() || systemSurface);
}

void SessionScene::attach(SurfaceWrapper *surface)
{
    if (m_surfaces.contains(surface))
        return;
    m_surfaces.append(surface);
    connect(surface, &SurfaceWrapper::surfaceRoleChanged, this, &SessionScene::refresh);
    connect(surface, &SurfaceWrapper::typeChanged, this, &SessionScene::refresh);
    connect(surface, &QObject::destroyed, this, &SessionScene::removeDestroyed);
    surface->setRenderingEnabled(allows(surface));
}

void SessionScene::removeDestroyed()
{
    m_surfaces.removeIf([](const auto &surface) { return surface.isNull(); });
}

void SessionScene::refresh()
{
    for (const auto &surface : std::as_const(m_surfaces)) {
        if (surface)
            surface->setRenderingEnabled(allows(surface));
    }
}

void SessionScene::activate() { refresh(); }
void SessionScene::deactivate() { refresh(); }

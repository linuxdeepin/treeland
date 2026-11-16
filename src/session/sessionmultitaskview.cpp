// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionmultitaskview.h"

#include "session.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "desktop/desktopenvironment.h"
#include "workspace/workspace.h"

SessionMultitaskView::SessionMultitaskView(Session *session, WaylandServer *runtime)
    : SessionComponent(session)
    , m_runtime(runtime)
    , m_desktop(session->desktop())
{
    initializeActiveState();
}

SessionMultitaskView::~SessionMultitaskView()
{
    reset();
}

IMultitaskView *SessionMultitaskView::view() const
{
    return qobject_cast<IMultitaskView *>(m_view.data());
}

qreal SessionMultitaskView::partialFactor() const
{
    auto *impl = view();
    return isActive() && impl ? impl->partialFactor() : 0.0;
}

void SessionMultitaskView::setStatus(IMultitaskView::Status status)
{
    if (isActive()) {
        if (auto *impl = view())
            impl->setStatus(status);
    }
}

void SessionMultitaskView::toggleMultitaskView(IMultitaskView::ActiveReason reason)
{
    if (!isActive() || !m_desktop || !m_runtime->isNormalOrMultitaskview())
        return;
    // A gesture finish must still run exit() after QML hides the view when
    // animations are disabled, so that focus and the runtime mode are restored.
    if (m_view && m_closing && reason != IMultitaskView::Gesture)
        return;
    if (!m_view) {
        m_closing = false;
        m_view = m_runtime->createMultitaskView(m_desktop);
        if (!m_view)
            return;
        Q_ASSERT(view());
        m_view->setZ(RootSurfaceContainer::OverlayZOrder);
        connect(m_view, &QQuickItem::visibleChanged,
                this, &SessionMultitaskView::onVisibleChanged);
        m_desktop->workspace()->setSwitcherEnabled(false);
    }
    if (auto *impl = view())
        impl->toggleMultitaskView(reason);
}

void SessionMultitaskView::updatePartialFactor(qreal progress)
{
    if (isActive()) {
        if (auto *impl = view())
            impl->updatePartialFactor(progress);
    }
}

void SessionMultitaskView::commitGesture(bool triggered)
{
    if (isActive()) {
        if (auto *impl = view())
            impl->commitGesture(triggered);
    }
}

void SessionMultitaskView::immediatelyExit()
{
    if (auto *impl = view())
        impl->immediatelyExit();
    reset();
}

void SessionMultitaskView::deactivate()
{
    immediatelyExit();
}

void SessionMultitaskView::onVisibleChanged()
{
    if (m_view && !m_view->isVisible()) {
        m_closing = true;
        if (m_desktop)
            m_desktop->workspace()->setSwitcherEnabled(true);
        m_view->deleteLater();
    }
}

void SessionMultitaskView::reset()
{
    if (m_view) {
        disconnect(m_view, nullptr, this, nullptr);
        delete m_view.data();
        m_view = nullptr;
    }

    m_closing = false;
    if (m_desktop && m_desktop->workspace())
        m_desktop->workspace()->setSwitcherEnabled(true);
}

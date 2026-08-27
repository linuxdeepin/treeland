// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wsgdamagetracker_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WSGDamageTracker::WSGDamageTracker(WSGDamageNode *root)
    : m_root(root)
{
}

WSGDamageTracker::~WSGDamageTracker() = default;

void WSGDamageTracker::setRoot(WSGDamageNode *root)
{
    if (m_root == root)
        return;
    m_root = root;
    m_settled = false;
    m_damage = { };
}

WSGDamageNode *WSGDamageTracker::root() const
{
    return m_root;
}

void WSGDamageTracker::addViewport(const std::shared_ptr<WSGViewport> &viewport)
{
    if (!viewport)
        return;

    bool exists = false;
    for (auto it = m_viewports.begin(); it != m_viewports.end();) {
        auto sp = it->lock();
        if (!sp) {
            it = m_viewports.erase(it);
            continue;
        }
        if (sp == viewport)
            exists = true;
        ++it;
    }

    if (!exists) {
        m_viewports.append(viewport);
    }
}

void WSGDamageTracker::removeViewport(WSGViewport *viewport)
{
    if (!viewport)
        return;

    for (auto it = m_viewports.begin(); it != m_viewports.end();) {
        auto sp = it->lock();
        if (!sp || sp.get() == viewport) {
            it = m_viewports.erase(it);
        } else {
            ++it;
        }
    }
}
void WSGDamageTracker::accumulateBackdropRecopy(const WSGDamageBackdropNode *node, const pixman_region32_t *damage)
{
    for (auto it = m_viewports.begin(); it != m_viewports.end(); ++it) {
        auto viewport = it->lock();
        if (!viewport)
            continue;

        auto *data = viewport->getAttachedData<WSGViewportBackdropData>();
        if (!data) {
            data = new WSGViewportBackdropData;
            viewport->setAttachedData<WSGViewportBackdropData>(data, [](void *p) {
                delete static_cast<WSGViewportBackdropData*>(p);
            });
            data->accumulate(node, node->recopySeed().native());
        } else {
            data->accumulate(node, damage);
        }
    }
}


void WSGDamageTracker::settle()
{
    if (m_settled)
        return;
    m_settled = true;
    m_damage = { };
    if (m_root && m_root->isDirty()) {
        WPixmanRegion worldDamage;
        WPixmanRegion backdropDamage;
        m_root->updateWorld(QMatrix4x4(), false, worldDamage, backdropDamage, nullptr, nullptr, this);
        m_damage = worldDamage;
        m_damage += backdropDamage;

        constexpr auto kShape = WSGDamageNode::DirtyMatrix | WSGDamageNode::DirtyGeometry
            | WSGDamageNode::DirtyAdded | WSGDamageNode::DirtyStructure
            | WSGDamageNode::DirtyVisibility | WSGDamageNode::DirtyOpaque
            | WSGDamageNode::DirtySubtreeGeometry;
        if (m_root->dirty() & kShape) {
            WPixmanRegion worldFrontOpaque;
            WPixmanRegion worldFrontBackdrop;
            m_root->computeWorldVisibility(worldFrontOpaque, worldFrontBackdrop);
        } else {
            m_root->commitStateRecursive();
        }
    }
}

void WSGDamageTracker::commit()
{
    settle();

    for (auto it = m_viewports.begin(); it != m_viewports.end();) {
        auto viewport = it->lock();
        if (!viewport) {
            it = m_viewports.erase(it);
            continue;
        }

        if (!viewport->isFull() && m_root && !m_damage.isEmpty())
            viewport->addDamage(WDamageRegion(m_damage, false));

        ++it;
    }

    m_settled = false;
    m_damage = { };
}


WAYLIB_SERVER_END_NAMESPACE

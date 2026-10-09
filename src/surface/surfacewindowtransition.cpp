// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfacewindowtransition.h"

#include "surface/surfacewrapper.h"

#include <wsurface.h>
#include <wsurfaceitem.h>

#include <QRectF>

SurfaceWindowTransition::SurfaceWindowTransition(SurfaceWrapper *wrapper)
    : WindowTransitionTarget()
    , m_wrapper(wrapper)
{
}

bool SurfaceWindowTransition::active() const
{
    return m_hasRect && !m_localRect.isEmpty();
}

WSurface *SurfaceWindowTransition::sourceSurface() const
{
    auto *source = m_source.data();
    return source ? source->surface() : nullptr;
}

std::optional<QRectF> SurfaceWindowTransition::globalRect() const
{
    if (!m_hasRect || !m_origin)
        return std::nullopt;

    const auto topLeft = m_origin->mapTransitionPointToGlobal(m_localRect.topLeft());
    if (!topLeft)
        return std::nullopt;

    return QRectF(*topLeft, m_localRect.size());
}

void SurfaceWindowTransition::begin()
{
    if (m_phase == Phase::Playing)
        return;

    m_phase = Phase::Playing;
    m_retainedOutputs = m_wrapper->outputs();
    if (m_source)
        m_source->retain(m_retainedOutputs);
}

void SurfaceWindowTransition::endAnimation(bool wrapperAboutToRemove)
{
    releaseSourceOutputs();

    if (wrapperAboutToRemove) {
        if (m_sourceDestroyConnection)
            QObject::disconnect(m_sourceDestroyConnection);
        m_sourceDestroyConnection = { };
        m_phase = Phase::Idle;
        m_hasRect = false;
        m_localRect = { };
        m_origin = nullptr;
        m_source = nullptr;
        Q_EMIT windowTransitionFinished();
    } else {
        m_phase = m_hasRect ? Phase::Armed : Phase::Idle;
    }
}

void SurfaceWindowTransition::resetWindowTransition()
{
    const bool wasArmed = m_hasRect;

    releaseSourceOutputs();
    if (m_sourceDestroyConnection)
        QObject::disconnect(m_sourceDestroyConnection);
    m_sourceDestroyConnection = { };
    m_source = nullptr;
    m_phase = Phase::Idle;
    m_hasRect = false;
    m_localRect = { };
    m_origin = nullptr;

    m_wrapper->updateWindowTransitionAnimationSource();

    if (wasArmed)
        Q_EMIT windowTransitionFinished();
}

void SurfaceWindowTransition::abort()
{
    releaseSourceOutputs();
    m_wrapper->updateWindowTransitionAnimationSource();
}

void SurfaceWindowTransition::setWindowTransitionRect(const QRectF &localRect,
                                                      WindowTransitionTarget *origin)
{
    // The rect is honored only when the surface is not yet mapped.
    if (m_wrapper->surface() && m_wrapper->surface()->mapped())
        return;

    m_localRect = localRect;
    m_origin = origin;
    m_hasRect = true;
    m_phase = Phase::Armed;
}

void SurfaceWindowTransition::updateWindowTransitionRect(const QRectF &localRect)
{
    if (!m_hasRect)
        return;

    m_localRect = localRect;
}

void SurfaceWindowTransition::setWindowTransitionSource(WindowTransitionSource *source)
{
    if (m_source.data() == source)
        return;

    if (m_sourceDestroyConnection)
        QObject::disconnect(m_sourceDestroyConnection);
    m_sourceDestroyConnection = { };
    m_source = source;
    if (source) {
        // The handle is owned by the manager and may outlive the animation;
        // track its surface so the QML item never keeps a dangling pointer.
        m_sourceDestroyConnection =
            QObject::connect(source->surface(),
                             &WSurface::beforeDestroy,
                             this,
                             &SurfaceWindowTransition::onSourceSurfaceDestroyed);
    }

    m_wrapper->updateWindowTransitionAnimationSource();
}

void SurfaceWindowTransition::clearWindowTransitionSource()
{
    releaseSourceOutputs();
    if (m_sourceDestroyConnection)
        QObject::disconnect(m_sourceDestroyConnection);
    m_sourceDestroyConnection = { };
    m_source = nullptr;

    m_wrapper->updateWindowTransitionAnimationSource();
}

std::optional<QPointF> SurfaceWindowTransition::mapTransitionPointToGlobal(
    const QPointF &surfacePoint) const
{
    auto *item = m_wrapper->surfaceItem();
    if (!item || !item->surface() || !item->shellSurface())
        return std::nullopt;

    return m_wrapper->position() + item->mapFromSurface(surfacePoint);
}

void SurfaceWindowTransition::releaseSourceOutputs()
{
    if (m_source && !m_retainedOutputs.isEmpty())
        m_source->release(m_retainedOutputs);
    m_retainedOutputs.clear();
}

void SurfaceWindowTransition::onSourceSurfaceDestroyed()
{
    m_sourceDestroyConnection = { };
    m_source = nullptr;
    m_retainedOutputs.clear();

    m_wrapper->updateWindowTransitionAnimationSource();
}

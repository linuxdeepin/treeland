// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "treelandconfig.h"

#include <qcoreevent.h>

DCORE_USE_NAMESPACE
TreelandConfig::TreelandConfig()
    : m_dconfig(DConfig::create("org.deepin.treeland", "org.deepin.treeland", QString()))
    , m_maxWorkspace(m_dconfig->value("maxWorkspace", 6).toUInt())
    , m_numWorkspace(m_dconfig->value("numWorkspace", 4).toUInt())
    , m_currentWorkspace(m_dconfig->value("currentWorkspace", 0).toUInt())
{
    connect(m_dconfig.get(), &DConfig::valueChanged, this, &TreelandConfig::onDConfigChanged);
}

uint TreelandConfig::workspaceThumbHeight() const
{
    return m_workspaceThumbHeight;
}

void TreelandConfig::setWorkspaceThumbHeight(uint newWorkspaceThumbHeight)
{
    if (m_workspaceThumbHeight == newWorkspaceThumbHeight)
        return;
    m_workspaceThumbHeight = newWorkspaceThumbHeight;
    Q_EMIT workspaceThumbHeightChanged();
    Q_EMIT workspaceDelegateHeightChanged();
}

uint TreelandConfig::workspaceThumbMargin() const
{
    return m_workspaceThumbMargin;
}

void TreelandConfig::setWorkspaceThumbMargin(uint newWorkspaceThumbMargin)
{
    if (m_workspaceThumbMargin == newWorkspaceThumbMargin)
        return;
    m_workspaceThumbMargin = newWorkspaceThumbMargin;
    Q_EMIT workspaceThumbMarginChanged();
    Q_EMIT workspaceDelegateHeightChanged();
}

uint TreelandConfig::workspaceDelegateHeight() const
{
    return workspaceThumbHeight() + 2 * workspaceThumbMargin();
}

uint TreelandConfig::workspaceThumbCornerRadius() const
{
    return m_workspaceThumbCornerRadius;
}

void TreelandConfig::setWorkspaceThumbCornerRadius(uint newWorkspaceThumbCornerRadius)
{
    if (m_workspaceThumbCornerRadius == newWorkspaceThumbCornerRadius)
        return;
    m_workspaceThumbCornerRadius = newWorkspaceThumbCornerRadius;
    emit workspaceThumbCornerRadiusChanged();
}

uint TreelandConfig::highlightBorderWidth() const
{
    return m_highlightBorderWidth;
}

void TreelandConfig::setHighlightBorderWidth(uint newHighlightBorderWidth)
{
    if (m_highlightBorderWidth == newHighlightBorderWidth)
        return;
    m_highlightBorderWidth = newHighlightBorderWidth;
    emit highlightBorderWidthChanged();
}

uint TreelandConfig::maxWorkspace() const
{
    return m_maxWorkspace;
}

void TreelandConfig::setMaxWorkspace(uint newMaxWorkspace)
{
    if (newMaxWorkspace == m_maxWorkspace)
        return;
    m_maxWorkspace = newMaxWorkspace;
    m_dconfig->setValue("maxWorkspace", QVariant::fromValue(m_maxWorkspace));
}

uint TreelandConfig::minMultitaskviewSurfaceHeight() const
{
    return m_minMultitaskviewSurfaceHeight;
}

void TreelandConfig::setMinMultitaskviewSurfaceHeight(uint newMinMultitaskviewSurfaceHeight)
{
    if (m_minMultitaskviewSurfaceHeight == newMinMultitaskviewSurfaceHeight)
        return;
    m_minMultitaskviewSurfaceHeight = newMinMultitaskviewSurfaceHeight;
    emit minMultitaskviewSurfaceHeightChanged();
}

uint TreelandConfig::titleBoxCornerRadius() const
{
    return m_titleBoxCornerRadius;
}

void TreelandConfig::setTitleBoxCornerRadius(uint newTitleBoxCornerRadius)
{
    if (m_titleBoxCornerRadius == newTitleBoxCornerRadius)
        return;
    m_titleBoxCornerRadius = newTitleBoxCornerRadius;
    emit titleBoxCornerRadiusChanged();
}

void TreelandConfig::onDConfigChanged(const QString &key)
{
    QByteArray baSignal = QStringLiteral("%1Changed()").arg(key).toLatin1();
    QByteArray baSignalName = QStringLiteral("%1Changed").arg(key).toLatin1();
    const char *signal = baSignal.data();
    const char *signalName = baSignalName.data();
    auto index = metaObject()->indexOfSignal(signal);
    if (index != -1)
        QMetaObject::invokeMethod(this, signalName, Qt::DirectConnection);
}

uint TreelandConfig::normalWindowHeight() const
{
    return m_normalWindowHeight;
}

void TreelandConfig::setNormalWindowHeight(uint newNormalWindowHeight)
{
    if (m_normalWindowHeight == newNormalWindowHeight)
        return;
    m_normalWindowHeight = newNormalWindowHeight;
    emit normalWindowHeightChanged();
}

uint TreelandConfig::windowHeightStep() const
{
    return m_windowHeightStep;
}

void TreelandConfig::setWindowHeightStep(uint newWindowHeightStep)
{
    if (m_windowHeightStep == newWindowHeightStep)
        return;
    m_windowHeightStep = newWindowHeightStep;
    emit windowHeightStepChanged();
}

uint TreelandConfig::numWorkspace() const
{
    return m_numWorkspace;
}

void TreelandConfig::setNumWorkspace(uint newNumWorkspace)
{
    if (newNumWorkspace == m_numWorkspace)
        return;
    m_numWorkspace = newNumWorkspace;
    m_dconfig->setValue("numWorkspace", QVariant::fromValue(m_numWorkspace));
}

uint TreelandConfig::currentWorkspace() const
{
    return m_currentWorkspace;
}

void TreelandConfig::setCurrentWorkspace(uint newCurrentWorkspace)
{
    if (newCurrentWorkspace == m_currentWorkspace)
        return;
    m_currentWorkspace = newCurrentWorkspace;
    m_dconfig->setValue("currentWorkspace", QVariant::fromValue(m_currentWorkspace));
}

qreal TreelandConfig::multitaskviewPaddingOpacity() const
{
    return m_multitaskviewPaddingOpacity;
}

void TreelandConfig::setMultitaskviewPaddingOpacity(qreal newMultitaskviewPaddingOpacity)
{
    if (qFuzzyCompare(m_multitaskviewPaddingOpacity, newMultitaskviewPaddingOpacity))
        return;
    m_multitaskviewPaddingOpacity = newMultitaskviewPaddingOpacity;
    emit multitaskviewPaddingOpacityChanged();
}

uint TreelandConfig::multitaskviewAnimationDuration() const
{
    return m_multitaskviewAnimationDuration;
}

void TreelandConfig::setMultitaskviewAnimationDuration(uint newMultitaskviewAnimationDuration)
{
    if (m_multitaskviewAnimationDuration == newMultitaskviewAnimationDuration)
        return;
    m_multitaskviewAnimationDuration = newMultitaskviewAnimationDuration;
    emit multitaskviewAnimationDurationChanged();
}

QEasingCurve::Type TreelandConfig::multitaskviewEasingCurveType() const
{
    return m_multitaskviewEasingCurveType;
}

void TreelandConfig::setMultitaskviewEasingCurveType(
    const QEasingCurve::Type &newMultitaskviewEasingCurveType)
{
    if (m_multitaskviewEasingCurveType == newMultitaskviewEasingCurveType)
        return;
    m_multitaskviewEasingCurveType = newMultitaskviewEasingCurveType;
    emit multitaskviewEasingCurveTypeChanged();
}

QString TreelandConfig::cursorThemeName() const
{
    return m_dconfig->value("cursorThemeName", "default").toString();
}

QSize TreelandConfig::cursorSize() const
{
    int size = m_dconfig->value("cursorSize", 24).toInt();
    return { size, size };
}

qreal TreelandConfig::windowRadius() const
{
    return m_dconfig->value("windowRadius", 18.0).toFloat();
}

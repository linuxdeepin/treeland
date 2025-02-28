// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "config/treelandconfig.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(qLcConfig, "treeland.config");

DCORE_USE_NAMESPACE
TreelandConfig::TreelandConfig()
{
    m_globleConfig = dconfig_org_deepin_treeland_globle::create(
        "org.deepin.treeland",
        "",
        this,
        &m_configThread
        );
    m_configThread.start();
}

dconfig_org_deepin_treeland_globle *TreelandConfig::globleConfig() const
{
    return m_globleConfig;
}

dconfig_org_deepin_treeland_user *TreelandConfig::currentUserConfig() const
{
    return m_currentUserConfig;
}

void TreelandConfig::setUserId(uint uid)
{
    if (m_userDconfig.contains(uid)) {
        m_currentUserConfig = m_userDconfig[uid];
    } else {
        m_currentUserConfig = dconfig_org_deepin_treeland_user::create(
            "org.deepin.treeland",
            QString::number(uid),
            this,
            &m_configThread
        );
        m_userDconfig[uid] = m_currentUserConfig;
    }
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

void TreelandConfig::setBlockActivateSurface(bool block)
{
    if (m_blockActivateSurface == block) {
        return;
    }

    m_blockActivateSurface = block;

    Q_EMIT blockActivateSurfaceChanged();
}

bool TreelandConfig::blockActivateSurface() const
{
    return m_blockActivateSurface;
}

uint TreelandConfig::multitaskviewTopContentMargin() const
{
    return m_multitaskviewTopContentMargin;
}

void TreelandConfig::setMultitaskviewTopContentMargin(uint newMultitaskviewTopContentMargin)
{
    if (m_multitaskviewTopContentMargin == newMultitaskviewTopContentMargin) {
        return;
    }
    m_multitaskviewTopContentMargin = newMultitaskviewTopContentMargin;
    Q_EMIT multitaskviewTopContentMarginChanged();
}

uint TreelandConfig::multitaskviewBottomContentMargin() const
{
    return m_multitaskviewBottomContentMargin;
}

void TreelandConfig::setMultitaskviewBottomContentMargin(uint newMultitaskviewBottomContentMargin)
{
    if (m_multitaskviewBottomContentMargin == newMultitaskviewBottomContentMargin) {
        return;
    }
    m_multitaskviewBottomContentMargin = newMultitaskviewBottomContentMargin;
    Q_EMIT multitaskviewBottomContentMarginChanged();
}

uint TreelandConfig::multitaskviewHorizontalMargin() const
{
    return m_multitaskviewHorizontalMargin;
}

void TreelandConfig::setMultitaskviewHorizontalMargin(uint newMultitaskviewHorizontalMargin)
{
    if (m_multitaskviewHorizontalMargin == newMultitaskviewHorizontalMargin) {
        return;
    }
    m_multitaskviewHorizontalMargin = newMultitaskviewHorizontalMargin;
    Q_EMIT multitaskviewHorizontalMarginChanged();
}

uint TreelandConfig::multitaskviewCellPadding() const
{
    return m_multitaskviewCellPadding;
}

void TreelandConfig::setMultitaskviewCellPadding(uint newMultitaskviewCellPadding)
{
    if (m_multitaskviewCellPadding == newMultitaskviewCellPadding) {
        return;
    }
    m_multitaskviewCellPadding = newMultitaskviewCellPadding;
    Q_EMIT multitaskviewCellPaddingChanged();
}

qreal TreelandConfig::multitaskviewLoadFactor() const
{
    return m_multitaskviewLoadFactor;
}

void TreelandConfig::setMultitaskviewLoadFactor(qreal newMultitaskviewLoadFactor)
{
    if (qFuzzyCompare(m_multitaskviewLoadFactor, newMultitaskviewLoadFactor))
        return;
    m_multitaskviewLoadFactor = newMultitaskviewLoadFactor;
    Q_EMIT multitaskviewLoadFactorChanged();
}

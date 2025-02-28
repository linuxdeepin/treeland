// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include "dconfig_org_deepin_treeland_user.hpp"
#include "dconfig_org_deepin_treeland_globle.hpp"

#include <DConfig>
#include <DSingleton>

#include <QEasingCurve>
#include <QObject>
#include <QQmlEngine>
#include <QSize>
#include <QThread>

class TreelandConfig
    : public QObject
    , public DTK_CORE_NAMESPACE::DSingleton<TreelandConfig>
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(uint workspaceThumbHeight READ workspaceThumbHeight WRITE setWorkspaceThumbHeight NOTIFY workspaceThumbHeightChanged FINAL)
    Q_PROPERTY(uint workspaceThumbMargin READ workspaceThumbMargin WRITE setWorkspaceThumbMargin NOTIFY workspaceThumbMarginChanged FINAL)
    Q_PROPERTY(uint workspaceDelegateHeight READ workspaceDelegateHeight NOTIFY workspaceDelegateHeightChanged FINAL)
    Q_PROPERTY(uint workspaceThumbCornerRadius READ workspaceThumbCornerRadius WRITE setWorkspaceThumbCornerRadius NOTIFY workspaceThumbCornerRadiusChanged FINAL)
    Q_PROPERTY(uint highlightBorderWidth READ highlightBorderWidth WRITE setHighlightBorderWidth NOTIFY highlightBorderWidthChanged FINAL)
    Q_PROPERTY(uint minMultitaskviewSurfaceHeight READ minMultitaskviewSurfaceHeight WRITE setMinMultitaskviewSurfaceHeight NOTIFY minMultitaskviewSurfaceHeightChanged FINAL)
    Q_PROPERTY(uint titleBoxCornerRadius READ titleBoxCornerRadius WRITE setTitleBoxCornerRadius NOTIFY titleBoxCornerRadiusChanged FINAL)
    Q_PROPERTY(uint normalWindowHeight READ normalWindowHeight WRITE setNormalWindowHeight NOTIFY normalWindowHeightChanged FINAL)
    Q_PROPERTY(uint windowHeightStep READ windowHeightStep WRITE setWindowHeightStep NOTIFY windowHeightStepChanged FINAL)
    Q_PROPERTY(qreal multitaskviewPaddingOpacity READ multitaskviewPaddingOpacity WRITE setMultitaskviewPaddingOpacity NOTIFY multitaskviewPaddingOpacityChanged FINAL)
    Q_PROPERTY(uint multitaskviewAnimationDuration READ multitaskviewAnimationDuration WRITE setMultitaskviewAnimationDuration NOTIFY multitaskviewAnimationDurationChanged FINAL)
    Q_PROPERTY(QEasingCurve::Type multitaskviewEasingCurveType READ multitaskviewEasingCurveType WRITE setMultitaskviewEasingCurveType NOTIFY multitaskviewEasingCurveTypeChanged FINAL)
    Q_PROPERTY(bool blockActivateSurface READ blockActivateSurface WRITE setBlockActivateSurface NOTIFY blockActivateSurfaceChanged FINAL)
    Q_PROPERTY(uint multitaskviewTopContentMargin READ multitaskviewTopContentMargin WRITE setMultitaskviewTopContentMargin NOTIFY multitaskviewTopContentMarginChanged FINAL)
    Q_PROPERTY(uint multitaskviewBottomContentMargin READ multitaskviewBottomContentMargin WRITE setMultitaskviewBottomContentMargin NOTIFY multitaskviewBottomContentMarginChanged FINAL)
    Q_PROPERTY(uint multitaskviewHorizontalMargin READ multitaskviewHorizontalMargin WRITE setMultitaskviewHorizontalMargin NOTIFY multitaskviewHorizontalMarginChanged FINAL)
    Q_PROPERTY(uint multitaskviewCellPadding READ multitaskviewCellPadding WRITE setMultitaskviewCellPadding NOTIFY multitaskviewCellPaddingChanged FINAL)
    Q_PROPERTY(qreal multitaskviewLoadFactor READ multitaskviewLoadFactor WRITE setMultitaskviewLoadFactor NOTIFY multitaskviewLoadFactorChanged FINAL)

public:
    TreelandConfig();

    dconfig_org_deepin_treeland_globle *globleConfig() const;
    dconfig_org_deepin_treeland_user *currentUserConfig() const;
    void setUserId(uint uid);

    uint workspaceThumbHeight() const;
    void setWorkspaceThumbHeight(uint newWorkspaceThumbHeight);

    uint workspaceThumbMargin() const;
    void setWorkspaceThumbMargin(uint newWorkspaceThumbMargin);

    uint workspaceDelegateHeight() const;

    uint workspaceThumbCornerRadius() const;
    void setWorkspaceThumbCornerRadius(uint newWorkspaceThumbCornerRadius);

    uint highlightBorderWidth() const;
    void setHighlightBorderWidth(uint newHighlightBorderWidth);

    uint minMultitaskviewSurfaceHeight() const;
    void setMinMultitaskviewSurfaceHeight(uint newMinMultitaskviewSurfaceHeight);

    uint titleBoxCornerRadius() const;
    void setTitleBoxCornerRadius(uint newTitleBoxCornerRadius);

    uint normalWindowHeight() const;
    void setNormalWindowHeight(uint newNormalWindowHeight);

    uint windowHeightStep() const;
    void setWindowHeightStep(uint newWindowHeightStep);

    qreal multitaskviewPaddingOpacity() const;
    void setMultitaskviewPaddingOpacity(qreal newMultitaskviewPaddingOpacity);

    uint multitaskviewAnimationDuration() const;
    void setMultitaskviewAnimationDuration(uint newMultitaskviewAnimationDuration);

    QEasingCurve::Type multitaskviewEasingCurveType() const;
    void setMultitaskviewEasingCurveType(const QEasingCurve::Type &newMultitaskviewEasingCurveType);

    void setBlockActivateSurface(bool block);
    bool blockActivateSurface() const;

    uint multitaskviewTopContentMargin() const;
    void setMultitaskviewTopContentMargin(uint newMultitaskviewTopContentMargin);

    uint multitaskviewBottomContentMargin() const;
    void setMultitaskviewBottomContentMargin(uint newMultitaskviewBottomContentMargin);

    uint multitaskviewHorizontalMargin() const;
    void setMultitaskviewHorizontalMargin(uint newMultitaskviewHorizontalMargin);

    uint multitaskviewCellPadding() const;
    void setMultitaskviewCellPadding(uint newMultitaskviewCellPadding);

    qreal multitaskviewLoadFactor() const;
    void setMultitaskviewLoadFactor(qreal newMultitaskviewLoadFactor);


Q_SIGNALS:
    void workspaceThumbMarginChanged();
    void workspaceThumbHeightChanged();
    void workspaceDelegateHeightChanged();
    void workspaceThumbCornerRadiusChanged();
    void highlightBorderWidthChanged();
    void minMultitaskviewSurfaceHeightChanged();
    void titleBoxCornerRadiusChanged();
    void normalWindowHeightChanged();
    void windowHeightStepChanged();
    void multitaskviewPaddingOpacityChanged();
    void multitaskviewAnimationDurationChanged();
    void multitaskviewEasingCurveTypeChanged();
    void windowRadiusChanged();
    void blockActivateSurfaceChanged();
    void multitaskviewTopContentMarginChanged();
    void multitaskviewBottomContentMarginChanged();
    void multitaskviewHorizontalMarginChanged();
    void multitaskviewCellPaddingChanged();
    void multitaskviewLoadFactorChanged();

private:
    QThread m_configThread;
    QMap<uint, dconfig_org_deepin_treeland_user*> m_userDconfig;
    dconfig_org_deepin_treeland_user *m_currentUserConfig = nullptr;
    dconfig_org_deepin_treeland_globle *m_globleConfig = nullptr;

    // Local
    uint m_workspaceThumbHeight = 144;
    uint m_workspaceThumbMargin = 20;
    uint m_workspaceThumbCornerRadius = 8;
    uint m_highlightBorderWidth = 4;
    uint m_minMultitaskviewSurfaceHeight = 232;
    uint m_titleBoxCornerRadius = 8;
    uint m_normalWindowHeight = 720;
    uint m_windowHeightStep = 15;
    qreal m_multitaskviewPaddingOpacity = 0.1;
    uint m_multitaskviewAnimationDuration = 300;
    QEasingCurve::Type m_multitaskviewEasingCurveType =
        QEasingCurve::OutExpo; // TODO: move to dconfig
    bool m_blockActivateSurface{ false };
    uint m_multitaskviewTopContentMargin = 40;
    uint m_multitaskviewBottomContentMargin = 60;
    uint m_multitaskviewHorizontalMargin = 20;
    uint m_multitaskviewCellPadding = 12;
    qreal m_multitaskviewLoadFactor = 0.6;
};

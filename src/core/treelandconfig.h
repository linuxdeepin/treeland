// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <DConfig>
#include <DSingleton>

#include <QEasingCurve>
#include <QObject>
#include <QQmlEngine>
#include <QSize>

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
    Q_PROPERTY(uint maxWorkspace READ maxWorkspace WRITE setMaxWorkspace NOTIFY maxWorkspaceChanged FINAL)
    Q_PROPERTY(uint numWorkspace READ numWorkspace WRITE setNumWorkspace NOTIFY numWorkspaceChanged FINAL)
    Q_PROPERTY(uint currentWorkspace READ currentWorkspace WRITE setCurrentWorkspace NOTIFY currentWorkspaceChanged FINAL)
    Q_PROPERTY(bool forceSoftwareCursor READ forceSoftwareCursor WRITE setForceSoftwareCursor NOTIFY forceSoftwareCursorChanged FINAL)
    Q_PROPERTY(uint minMultitaskviewSurfaceHeight READ minMultitaskviewSurfaceHeight WRITE setMinMultitaskviewSurfaceHeight NOTIFY minMultitaskviewSurfaceHeightChanged FINAL)
    Q_PROPERTY(uint titleBoxCornerRadius READ titleBoxCornerRadius WRITE setTitleBoxCornerRadius NOTIFY titleBoxCornerRadiusChanged FINAL)
    Q_PROPERTY(uint normalWindowHeight READ normalWindowHeight WRITE setNormalWindowHeight NOTIFY normalWindowHeightChanged FINAL)
    Q_PROPERTY(uint windowHeightStep READ windowHeightStep WRITE setWindowHeightStep NOTIFY windowHeightStepChanged FINAL)
    Q_PROPERTY(qreal multitaskviewPaddingOpacity READ multitaskviewPaddingOpacity WRITE setMultitaskviewPaddingOpacity NOTIFY multitaskviewPaddingOpacityChanged FINAL)
    Q_PROPERTY(uint multitaskviewAnimationDuration READ multitaskviewAnimationDuration WRITE setMultitaskviewAnimationDuration NOTIFY multitaskviewAnimationDurationChanged FINAL)
    Q_PROPERTY(QEasingCurve::Type multitaskviewEasingCurveType READ multitaskviewEasingCurveType WRITE setMultitaskviewEasingCurveType NOTIFY multitaskviewEasingCurveTypeChanged FINAL)
    Q_PROPERTY(QString cursorThemeName READ cursorThemeName NOTIFY cursorThemeNameChanged FINAL)
    Q_PROPERTY(QSize cursorSize READ cursorSize NOTIFY cursorSizeChanged FINAL)
    Q_PROPERTY(qreal windowRadius READ windowRadius NOTIFY windowRadiusChanged FINAL)
    Q_PROPERTY(QString activeColor READ activeColor WRITE setActiveColor NOTIFY activeColorChanged FINAL)
    Q_PROPERTY(uint windowOpacity READ windowOpacity WRITE setWindowOpacity NOTIFY windowOpacityChanged FINAL)
    Q_PROPERTY(uint windowThemeType READ windowThemeType WRITE setWindowThemeType NOTIFY windowThemeTypeChanged FINAL)
    Q_PROPERTY(uint windowTitlebarHeight READ windowTitlebarHeight WRITE setWindowTitlebarHeight NOTIFY windowTitlebarHeightChanged FINAL)
    Q_PROPERTY(bool blockActivateSurface READ blockActivateSurface WRITE setBlockActivateSurface NOTIFY blockActivateSurfaceChanged FINAL)

public:
    TreelandConfig();

    uint workspaceThumbHeight() const;
    void setWorkspaceThumbHeight(uint newWorkspaceThumbHeight);

    uint workspaceThumbMargin() const;
    void setWorkspaceThumbMargin(uint newWorkspaceThumbMargin);

    uint workspaceDelegateHeight() const;

    uint workspaceThumbCornerRadius() const;
    void setWorkspaceThumbCornerRadius(uint newWorkspaceThumbCornerRadius);

    uint highlightBorderWidth() const;
    void setHighlightBorderWidth(uint newHighlightBorderWidth);

    uint maxWorkspace() const;
    void setMaxWorkspace(uint newMaxWorkspace);

    uint minMultitaskviewSurfaceHeight() const;
    void setMinMultitaskviewSurfaceHeight(uint newMinMultitaskviewSurfaceHeight);

    uint titleBoxCornerRadius() const;
    void setTitleBoxCornerRadius(uint newTitleBoxCornerRadius);

    uint normalWindowHeight() const;
    void setNormalWindowHeight(uint newNormalWindowHeight);

    uint windowHeightStep() const;
    void setWindowHeightStep(uint newWindowHeightStep);

    uint numWorkspace() const;
    void setNumWorkspace(uint newNumWorkspace);

    uint currentWorkspace() const;
    void setCurrentWorkspace(uint newCurrentWorkspace);

    bool forceSoftwareCursor();
    void setForceSoftwareCursor(bool enable);

    qreal multitaskviewPaddingOpacity() const;
    void setMultitaskviewPaddingOpacity(qreal newMultitaskviewPaddingOpacity);

    uint multitaskviewAnimationDuration() const;
    void setMultitaskviewAnimationDuration(uint newMultitaskviewAnimationDuration);

    QEasingCurve::Type multitaskviewEasingCurveType() const;
    void setMultitaskviewEasingCurveType(const QEasingCurve::Type &newMultitaskviewEasingCurveType);

    QString cursorThemeName() const;

    QSize cursorSize() const;

    qreal windowRadius() const;

    void setActiveColor(const QString &color);
    QString activeColor();

    void setWindowOpacity(uint32_t opacity);
    uint32_t windowOpacity();

    void setWindowThemeType(uint32_t type);
    uint32_t windowThemeType();

    void setWindowTitlebarHeight(uint titlebarHeight);
    uint32_t windowTitlebarHeight();

    void setBlockActivateSurface(bool block);
    bool blockActivateSurface() const;

Q_SIGNALS:
    void workspaceThumbMarginChanged();
    void workspaceThumbHeightChanged();
    void workspaceDelegateHeightChanged();
    void workspaceThumbCornerRadiusChanged();
    void highlightBorderWidthChanged();
    void maxWorkspaceChanged();
    void minMultitaskviewSurfaceHeightChanged();
    void titleBoxCornerRadiusChanged();
    void normalWindowHeightChanged();
    void windowHeightStepChanged();
    void numWorkspaceChanged();
    void currentWorkspaceChanged();
    void forceSoftwareCursorChanged();
    void multitaskviewPaddingOpacityChanged();
    void multitaskviewAnimationDurationChanged();
    void multitaskviewEasingCurveTypeChanged();
    void cursorThemeNameChanged();
    void cursorSizeChanged();
    void windowRadiusChanged();
    void activeColorChanged();
    void windowOpacityChanged();
    void windowThemeTypeChanged();
    void windowTitlebarHeightChanged();
    void blockActivateSurfaceChanged();

private:
    void onDConfigChanged(const QString &key);

    // DConfig
    std::unique_ptr<DTK_CORE_NAMESPACE::DConfig> m_dconfig;
    uint m_maxWorkspace;
    uint m_numWorkspace;
    uint m_currentWorkspace;
    bool m_forceSoftwareCursor;
    QString m_activeColor;
    uint32_t m_windowOpacity;
    uint32_t m_windowThemeType;
    uint32_t m_windowTitlebarHeight;

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
        QEasingCurve::OutQuad; // TODO: move to dconfig
    bool m_blockActivateSurface{ false };
};

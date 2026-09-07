// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-treeland-virtual-output-manager-v1.h"
#include "qwayland-treeland-output-manager-v1.h"

#include <QMap>
#include <QObject>
#include <QSharedPointer>
#include <QWaylandClientExtension>
#include <memory>

class QWidget;
class QLabel;
class QComboBox;
class QSlider;
class QSpinBox;
class QPushButton;
class VirtualOutputManager;
class VirtualOutput;
class WlrOutputManager;
class ColorControl;

// Client wrapper for treeland_output_manager_v1 (color control).
class TreelandOutputManager
    : public QWaylandClientExtensionTemplate<TreelandOutputManager>
    , public QtWayland::treeland_output_manager_v1
{
    Q_OBJECT
public:
    explicit TreelandOutputManager();
    ~TreelandOutputManager() override;

    void instantiate();

    // Get the color control object for a given wl_output. Returns nullptr if
    // the manager is not active.
    struct ::treeland_output_color_control_v1 *getColorControl(struct ::wl_output *output);

Q_SIGNALS:
    // The compositor pushes the current primary output name whenever it
    // changes (including auto-restore after a disable/enable cycle).
    void primaryOutputChanged(const QString &name);

protected:
    void treeland_output_manager_v1_primary_output(const QString &output_name) override
    {
        Q_EMIT primaryOutputChanged(output_name);
    }

private:
    // Cache of color control wrappers per wl_output
    // (managed in virtualclient.cpp, not here)
};

// Main UI controller for the virtual output test tool.
class VirtualClient : public QObject
{
    Q_OBJECT
public:
    explicit VirtualClient(QObject *parent = nullptr);
    ~VirtualClient();

private:
    void setupUi();
    void refreshScreenInfo();
    void refreshMirrorCombo();
    void onScreenChanged(int index);
    void onMirrorChanged(int index);
    void onDisableClicked();
    void onPrimaryClicked();
    void onResolutionChanged(int resIndex);
    void onBrightnessChanged(int value);
    void onColorTempChanged(int value);
    void onPrimaryOutputChanged(const QString &name);
    void updatePrimaryButtonText();
    void applyCurrentSettings();
    void onColorControlBrightness(int percent);
    void onColorControlColorTemp(uint32_t kelvin);
    // Rebind after mirror changes replace wl_output proxies.
    void acquireColorControl(const QString &screenName);
    void onWlrHeadsChanged();
    void onVirtualOutputModified(const QString &name);
    void onWlrApplyFinished(bool ok);
    void onVirtualOutputListReceived(const QStringList &names);
    void onVirtualOutputOutputs(const QString &name, const QStringList &outputs);

    // Create or update mirror group for the selected screen
    void updateMirrorGroup();

    // Get wl_output pointer for a screen name
    struct ::wl_output *getWlOutput(const QString &name);

    std::unique_ptr<VirtualOutputManager> m_virtualManager;
    std::unique_ptr<TreelandOutputManager> m_treelandManager;
    std::unique_ptr<WlrOutputManager> m_wlrManager;

    QWidget *m_widget = nullptr;
    QLabel *m_infoLabel = nullptr;
    QComboBox *m_screenCombo = nullptr;
    QComboBox *m_mirrorCombo = nullptr;
    QPushButton *m_disableBtn = nullptr;
    QPushButton *m_primaryBtn = nullptr;
    QLabel *m_primaryLabel = nullptr;

    // Settings controls
    QSlider *m_brightnessSlider = nullptr;
    QLabel *m_brightnessLabel = nullptr;
    QSlider *m_colorTempSlider = nullptr;
    QLabel *m_colorTempLabel = nullptr;
    QSpinBox *m_posX = nullptr;
    QSpinBox *m_posY = nullptr;
    QComboBox *m_resolutionCombo = nullptr;
    QComboBox *m_refreshCombo = nullptr;
    QComboBox *m_rotationCombo = nullptr;
    QComboBox *m_scaleCombo = nullptr;
    ColorControl *m_colorControl = nullptr;
    struct ::wl_output *m_colorOutput = nullptr;
    // Includes disabled heads but excludes unplugged ones.
    QStringList m_screenNames;
    QString m_primaryName;
    // Suppresses applies while populating controls.
    bool m_updatingUi = false;
    QMap<QString, QStringList> m_mirrorGroups;
    QMap<QString, QString> m_screenToGroup;
    QMap<QString, QSharedPointer<VirtualOutput>> m_virtualOutputs;
};
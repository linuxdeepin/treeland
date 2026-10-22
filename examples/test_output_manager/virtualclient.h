// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "outputuuid.h"
#include "qwayland-treeland-output-manager-unstable-v2.h"

#include <QHash>
#include <QMap>
#include <QObject>
#include <QWaylandClientExtension>
#include <memory>

class QWidget;
class QLabel;
class QComboBox;
class QSlider;
class QSpinBox;
class QPushButton;
class QTimer;
class OutputGroupManager;
class OutputGroup;
class WlrOutputManager;
class ColorControl;

// v2 addresses outputs by the uuid served by treeland-output-uuid-v1.
class TreelandOutputManager
    : public QWaylandClientExtensionTemplate<TreelandOutputManager>
    , public QtWayland::treeland_output_manager_v2
{
    Q_OBJECT
public:
    explicit TreelandOutputManager();
    ~TreelandOutputManager() override;

    void instantiate();

    struct ::treeland_output_picture_control_v2 *getPictureControl(const QString &uuid);

Q_SIGNALS:
    void primaryOutputChanged(const QString &uuid);

protected:
    void treeland_output_manager_v2_primary_output(const QString &uuid) override
    {
        Q_EMIT primaryOutputChanged(uuid);
    }

private:
};

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
    void onBrightnessSliderReleased();
    void onColorTempSliderReleased();
    void commitBrightness();
    void commitColorTemp();
    void onPrimaryOutputChanged(const QString &uuid);
    void updatePrimaryButtonText();
    void applyCurrentSettings();
    void onColorControlBrightness(int percent);
    void onColorControlColorTemp(uint32_t kelvin);
    void acquireColorControl(const QString &screenName);
    void refreshUuids();
    void onUuidReceived(const QString &name, const QString &uuid);
    QString uuidForScreen(const QString &name) const;
    QString screenForUuid(const QString &uuid) const;
    void updatePrimaryLabel();
    void onWlrHeadsChanged();
    void onWlrApplyFinished(bool ok);

    void onGroupAdded(const QString &name);
    void onGroupsDone();
    void onGroupRemoved(const QString &name);
    void onGroupState(const QString &groupName,
                      const QString &sourceUuid,
                      const QStringList &mirrorUuids);

    void updateMirrorGroup();
    void attachGroup(const QString &name);
    void updateScreenToGroup();

    std::unique_ptr<OutputGroupManager> m_groupManager;
    std::unique_ptr<TreelandOutputManager> m_treelandManager;
    std::unique_ptr<WlrOutputManager> m_wlrManager;
    std::unique_ptr<OutputUuidManagerV1> m_uuidManager;

    QWidget *m_widget = nullptr;
    QLabel *m_infoLabel = nullptr;
    QComboBox *m_screenCombo = nullptr;
    QComboBox *m_mirrorCombo = nullptr;
    QPushButton *m_disableBtn = nullptr;
    QPushButton *m_primaryBtn = nullptr;
    QLabel *m_primaryLabel = nullptr;

    QSlider *m_brightnessSlider = nullptr;
    QLabel *m_brightnessLabel = nullptr;
    QSlider *m_colorTempSlider = nullptr;
    QLabel *m_colorTempLabel = nullptr;
    QTimer *m_brightnessCommitTimer = nullptr;
    QTimer *m_colorTempCommitTimer = nullptr;
    QSpinBox *m_posX = nullptr;
    QSpinBox *m_posY = nullptr;
    QComboBox *m_resolutionCombo = nullptr;
    QComboBox *m_refreshCombo = nullptr;
    QComboBox *m_rotationCombo = nullptr;
    QComboBox *m_scaleCombo = nullptr;
    ColorControl *m_colorControl = nullptr;
    QStringList m_screenNames;
    QString m_primaryUuid;
    QHash<QString, OutputUuidV1 *> m_uuidObjects;
    QHash<QString, QString> m_uuidByName;
    bool m_updatingUi = false;
    QMap<QString, QStringList> m_mirrorGroups;
    QMap<QString, QString> m_screenToGroup;
    QMap<QString, OutputGroup *> m_groups;
    QStringList m_groupNames;
    QString m_pendingMirrorScreen;
};

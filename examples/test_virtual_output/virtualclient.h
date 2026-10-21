// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-treeland-output-uuid-unstable-v1.h"
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
class OutputGroupManager;
class OutputGroup;

class OutputUuidManager
    : public QWaylandClientExtensionTemplate<OutputUuidManager>
    , public QtWayland::treeland_output_uuid_manager_v1
{
    Q_OBJECT
public:
    explicit OutputUuidManager();
    ~OutputUuidManager() override;

    void instantiate();

    struct ::treeland_output_uuid_v1 *forOutput(struct ::wl_output *output);
};

class OutputUuid
    : public QObject
    , public QtWayland::treeland_output_uuid_v1
{
    Q_OBJECT
public:
    explicit OutputUuid(struct ::treeland_output_uuid_v1 *object, QObject *parent = nullptr);
    ~OutputUuid() override;

Q_SIGNALS:
    void uuidReceived(const QString &uuid);

protected:
    void treeland_output_uuid_v1_uuid(const QString &uuid) override;
};
class WlrOutputManager;
class ColorControl;

class TreelandOutputManager
    : public QWaylandClientExtensionTemplate<TreelandOutputManager>
    , public QtWayland::treeland_output_manager_v1
{
    Q_OBJECT
public:
    explicit TreelandOutputManager();
    ~TreelandOutputManager() override;

    void instantiate();

    struct ::treeland_output_color_control_v1 *getColorControl(struct ::wl_output *output);

Q_SIGNALS:
    void primaryOutputChanged(const QString &name);

protected:
    void treeland_output_manager_v1_primary_output(const QString &output_name) override
    {
        Q_EMIT primaryOutputChanged(output_name);
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
    void onPrimaryOutputChanged(const QString &name);
    void updatePrimaryButtonText();
    void applyCurrentSettings();
    void onColorControlBrightness(int percent);
    void onColorControlColorTemp(uint32_t kelvin);
    void acquireColorControl(const QString &screenName);
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
    void refreshScreenUuids();

    QString uuidForScreen(const QString &screenName) const;
    QString screenForUuid(const QString &uuid) const;

    struct ::wl_output *getWlOutput(const QString &name);

    std::unique_ptr<OutputGroupManager> m_groupManager;
    std::unique_ptr<OutputUuidManager> m_uuidManager;
    std::unique_ptr<TreelandOutputManager> m_treelandManager;
    std::unique_ptr<WlrOutputManager> m_wlrManager;

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
    QSpinBox *m_posX = nullptr;
    QSpinBox *m_posY = nullptr;
    QComboBox *m_resolutionCombo = nullptr;
    QComboBox *m_refreshCombo = nullptr;
    QComboBox *m_rotationCombo = nullptr;
    QComboBox *m_scaleCombo = nullptr;
    ColorControl *m_colorControl = nullptr;
    QStringList m_screenNames;
    QString m_primaryName;
    bool m_updatingUi = false;
    QMap<QString, QStringList> m_mirrorGroups;
    QMap<QString, QString> m_screenToGroup;
    QMap<QString, OutputGroup *> m_groups;
    QStringList m_groupNames;
    QString m_pendingMirrorScreen;
    QMap<QString, QString> m_screenUuids;
};
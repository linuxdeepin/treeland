// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualclient.h"
#include "outputgroup.h"
#include "outputgroupmanager.h"
#include "wlroutputmanager.h"

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSet>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtWaylandClient/QWaylandClientExtension>

#include <wayland-client-core.h>
#include <cmath>


TreelandOutputManager::TreelandOutputManager()
    : QWaylandClientExtensionTemplate<TreelandOutputManager>(1)
{
}

TreelandOutputManager::~TreelandOutputManager()
{
    if (isInitialized())
        destroy();
}

void TreelandOutputManager::instantiate()
{
    initialize();
}

struct ::treeland_output_picture_control_v2 *TreelandOutputManager::getPictureControl(const QString &uuid)
{
    if (!isInitialized())
        return nullptr;
    return get_picture_control(uuid);
}


class ColorControl : public QObject, public QtWayland::treeland_output_picture_control_v2
{
    Q_OBJECT
public:
    ColorControl(struct ::treeland_output_picture_control_v2 *obj, QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_output_picture_control_v2(obj)
    {
    }
    ~ColorControl() override
    {
        if (isInitialized())
            destroy();
    }

Q_SIGNALS:
    void brightnessChanged(int percent);
    void colorTemperatureChanged(uint32_t kelvin);

protected:
    void treeland_output_picture_control_v2_brightness(wl_fixed_t b) override
    {
        Q_EMIT brightnessChanged(static_cast<int>(std::round(wl_fixed_to_double(b))));
    }
    void treeland_output_picture_control_v2_color_temperature(uint32_t t) override
    {
        Q_EMIT colorTemperatureChanged(t);
    }
    void treeland_output_picture_control_v2_result(uint32_t result) override
    {
        // Carried over from the removed test-color-control example: report what
        // the compositor answered for the last commit.
        QString text;
        switch (result) {
        case commit_result_success:
            text = "success";
            break;
        case commit_result_failed:
            text = "failed";
            break;
        case commit_result_unsupported:
            text = "unsupported";
            break;
        case commit_result_invalid_output:
            text = "invalid_output";
            break;
        default:
            text = QString::number(result);
            break;
        }
        qInfo() << "picture control commit result:" << text;
    }
};

VirtualClient::VirtualClient(QObject *parent)
    : QObject(parent)
{
    m_groupManager = std::make_unique<OutputGroupManager>();
    m_treelandManager = std::make_unique<TreelandOutputManager>();
    m_wlrManager = std::make_unique<WlrOutputManager>();
    m_uuidManager = std::make_unique<OutputUuidManagerV1>();

    connect(m_groupManager.get(), &OutputGroupManager::activeChanged, this, [this]() {
        if (m_groupManager->isActive())
            setupUi();
    });
    connect(m_groupManager.get(), &OutputGroupManager::groupAdded,
            this, &VirtualClient::onGroupAdded);
    connect(m_groupManager.get(), &OutputGroupManager::groupsDone,
            this, &VirtualClient::onGroupsDone);
    connect(m_groupManager.get(), &OutputGroupManager::groupRemoved,
            this, &VirtualClient::onGroupRemoved);

    connect(m_treelandManager.get(), &TreelandOutputManager::primaryOutputChanged,
            this, &VirtualClient::onPrimaryOutputChanged);

    connect(m_wlrManager.get(), &WlrOutputManager::headsChanged, this, &VirtualClient::onWlrHeadsChanged);
    connect(m_wlrManager.get(), &WlrOutputManager::applyFinished, this, &VirtualClient::onWlrApplyFinished);

    connect(m_uuidManager.get(), &OutputUuidManagerV1::activeChanged, this, [this]() {
        if (m_uuidManager->isActive())
            refreshUuids();
    });

    m_groupManager->instantiate();
    m_treelandManager->instantiate();
    m_wlrManager->instantiate();
    m_uuidManager->instantiate();
}

VirtualClient::~VirtualClient()
{
    delete m_colorControl;
}

void VirtualClient::setupUi()
{
    if (m_widget)
        return;

    m_widget = new QWidget;
    m_widget->resize(640, 700);

    auto *mainLayout = new QVBoxLayout(m_widget);
    mainLayout->setSpacing(8);

    m_infoLabel = new QLabel("Loading screen info...");
    m_infoLabel->setWordWrap(true);
    mainLayout->addWidget(m_infoLabel);

    auto *selRow = new QHBoxLayout;
    selRow->addWidget(new QLabel("Screen:"));
    m_screenCombo = new QComboBox;
    m_screenCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selRow->addWidget(m_screenCombo);

    m_primaryBtn = new QPushButton("Primary");
    connect(m_primaryBtn, &QPushButton::clicked, this, &VirtualClient::onPrimaryClicked);
    selRow->addWidget(m_primaryBtn);

    m_primaryLabel = new QLabel("Primary: —");
    m_primaryLabel->setStyleSheet("color: #4a90d9;");
    selRow->addWidget(m_primaryLabel);

    m_disableBtn = new QPushButton("Disable");
    connect(m_disableBtn, &QPushButton::clicked, this, &VirtualClient::onDisableClicked);
    selRow->addWidget(m_disableBtn);
    selRow->addWidget(new QLabel("Mirror:"));
    m_mirrorCombo = new QComboBox;
    m_mirrorCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selRow->addWidget(m_mirrorCombo);
    mainLayout->addLayout(selRow);

    auto *settingsGroup = new QGroupBox("Screen Settings");
    auto *form = new QFormLayout(settingsGroup);
    form->setSpacing(6);

    m_brightnessSlider = new QSlider(Qt::Horizontal);
    m_brightnessSlider->setRange(0, 100);
    m_brightnessSlider->setValue(100);
    m_brightnessLabel = new QLabel("100%");
    // valueChanged only refreshes the local label; the actual commit is
    // debounced (and also fired on sliderReleased) so dragging the slider does
    // not flood the compositor with one commit per intermediate value.
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &VirtualClient::onBrightnessChanged);
    connect(m_brightnessSlider, &QSlider::sliderReleased, this, &VirtualClient::onBrightnessSliderReleased);
    m_brightnessCommitTimer = new QTimer(this);
    m_brightnessCommitTimer->setSingleShot(true);
    m_brightnessCommitTimer->setInterval(120);
    connect(m_brightnessCommitTimer, &QTimer::timeout, this, &VirtualClient::commitBrightness);
    auto *brightRow = new QHBoxLayout;
    brightRow->addWidget(m_brightnessSlider);
    brightRow->addWidget(m_brightnessLabel);
    form->addRow("Brightness:", brightRow);

    m_colorTempSlider = new QSlider(Qt::Horizontal);
    m_colorTempSlider->setRange(1000, 20000);
    m_colorTempSlider->setValue(6500);
    m_colorTempLabel = new QLabel("6500K");
    connect(m_colorTempSlider, &QSlider::valueChanged, this, &VirtualClient::onColorTempChanged);
    connect(m_colorTempSlider, &QSlider::sliderReleased, this, &VirtualClient::onColorTempSliderReleased);
    m_colorTempCommitTimer = new QTimer(this);
    m_colorTempCommitTimer->setSingleShot(true);
    m_colorTempCommitTimer->setInterval(120);
    connect(m_colorTempCommitTimer, &QTimer::timeout, this, &VirtualClient::commitColorTemp);
    auto *ctRow = new QHBoxLayout;
    ctRow->addWidget(m_colorTempSlider);
    ctRow->addWidget(m_colorTempLabel);
    form->addRow("Color Temp:", ctRow);

    m_posX = new QSpinBox;
    m_posX->setRange(-99999, 99999);
    m_posY = new QSpinBox;
    m_posY->setRange(-99999, 99999);
    auto *posRow = new QHBoxLayout;
    posRow->addWidget(new QLabel("X:"));
    posRow->addWidget(m_posX);
    posRow->addWidget(new QLabel("Y:"));
    posRow->addWidget(m_posY);
    form->addRow("Position:", posRow);

    m_resolutionCombo = new QComboBox;
    form->addRow("Resolution:", m_resolutionCombo);

    m_refreshCombo = new QComboBox;
    form->addRow("Refresh:", m_refreshCombo);

    m_rotationCombo = new QComboBox;
    m_rotationCombo->addItem("Normal", 0);
    m_rotationCombo->addItem("90°", 1);
    m_rotationCombo->addItem("180°", 2);
    m_rotationCombo->addItem("270°", 3);
    m_rotationCombo->addItem("Flipped", 4);
    m_rotationCombo->addItem("Flipped 90°", 5);
    m_rotationCombo->addItem("Flipped 180°", 6);
    m_rotationCombo->addItem("Flipped 270°", 7);
    form->addRow("Rotation:", m_rotationCombo);

    m_scaleCombo = new QComboBox;
    for (double s : {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0})
        m_scaleCombo->addItem(QString::number(s, 'f', 2), s);
    m_scaleCombo->setCurrentText("1.00");
    form->addRow("Scale:", m_scaleCombo);

    mainLayout->addWidget(settingsGroup);

    mainLayout->addStretch();

    connect(m_screenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualClient::onScreenChanged);
    connect(m_mirrorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualClient::onMirrorChanged);
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualClient::onResolutionChanged);

    connect(m_posX, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VirtualClient::applyCurrentSettings);
    connect(m_posY, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VirtualClient::applyCurrentSettings);
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualClient::applyCurrentSettings);
    connect(m_refreshCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualClient::applyCurrentSettings);
    connect(m_rotationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualClient::applyCurrentSettings);
    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VirtualClient::applyCurrentSettings);
    m_widget->show();

    refreshScreenInfo();
}

void VirtualClient::refreshUuids()
{
    if (!m_uuidManager->isActive())
        return;

    for (QScreen *screen : QGuiApplication::screens()) {
        const QString name = screen->name();
        if (name.isEmpty() || m_uuidObjects.contains(name))
            continue;

        auto *native = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
        if (!native || !native->output())
            continue;

        auto *object = m_uuidManager->get_uuid(native->output());
        if (!object)
            continue;

        auto *probe = new OutputUuidV1(object, QStringLiteral("wl_output"), name, true, this);
        connect(probe, &OutputUuidV1::uuidReceived, this,
                [this, name](OutputUuidV1 *, const QString &uuid) {
                    onUuidReceived(name, uuid);
                });
        m_uuidObjects.insert(name, probe);
    }
}

void VirtualClient::onUuidReceived(const QString &name, const QString &uuid)
{
    m_uuidByName[name] = uuid;
    updatePrimaryLabel();
    updatePrimaryButtonText();
    // A pending mirror request was waiting for this uuid (or the one of the
    // screen being mirrored); re-run it now that the mapping is known.
    if (!m_pendingMirrorScreen.isEmpty())
        QMetaObject::invokeMethod(this, &VirtualClient::updateMirrorGroup, Qt::QueuedConnection);
    if (m_screenCombo && m_screenCombo->currentText() == name)
        acquireColorControl(name);
}

QString VirtualClient::uuidForScreen(const QString &name) const
{
    return m_uuidByName.value(name);
}

QString VirtualClient::screenForUuid(const QString &uuid) const
{
    if (uuid.isEmpty())
        return QString();
    for (auto it = m_uuidByName.cbegin(); it != m_uuidByName.cend(); ++it) {
        if (it.value() == uuid)
            return it.key();
    }
    return QString();
}

void VirtualClient::refreshScreenInfo()
{
    if (!m_widget)
        return;

    QString info;
    info += "<b>Physical Screens:</b><br>";

    const auto screens = QGuiApplication::screens();
    for (auto *screen : screens) {
        info += QString("  %1 — %2x%3 @ %4Hz, pos(%5,%6), scale %7<br>")
                    .arg(screen->name(),
                         QString::number(screen->geometry().width()),
                         QString::number(screen->geometry().height()),
                         QString::number(screen->refreshRate(), 'f', 1),
                         QString::number(screen->geometry().x()),
                         QString::number(screen->geometry().y()),
                         QString::number(screen->devicePixelRatio(), 'f', 1));
    }

    auto wlrHeads = m_wlrManager->heads();
    if (!wlrHeads.isEmpty()) {
        info += "<br><b>Wlr Heads:</b><br>";
        for (const auto &h : wlrHeads) {
            info += QString("  %1 — %2x%3 @ %4, pos(%5,%6), scale %7, rot %8<br>")
                        .arg(h.name,
                             h.currentMode >= 0 && h.currentMode < h.modes.size()
                                 ? QString::number(h.modes[h.currentMode].size.width())
                                 : "?",
                             h.currentMode >= 0 && h.currentMode < h.modes.size()
                                 ? QString::number(h.modes[h.currentMode].size.height())
                                 : "?",
                             h.currentMode >= 0 && h.currentMode < h.modes.size()
                                 ? QString::number(h.modes[h.currentMode].refresh / 1000.0, 'f', 1)
                                 : "?",
                             QString::number(h.position.x()),
                             QString::number(h.position.y()),
                             QString::number(h.scale, 'f', 2),
                             QString::number(h.transform));
        }
    }

    info += "<br><b>Mirror Groups:</b><br>";
    if (m_mirrorGroups.isEmpty()) {
        info += "  (none)<br>";
    } else {
        for (auto it = m_mirrorGroups.begin(); it != m_mirrorGroups.end(); ++it) {
            const auto &outputs = it.value();
            if (outputs.size() >= 2) {
                QStringList mirrors;
                for (int i = 1; i < outputs.size(); ++i)
                    mirrors << outputs[i];
                info += QString("  %1 ← %2<br>").arg(outputs.first(), mirrors.join(", "));
            }
        }
    }

    m_infoLabel->setText(info);

    QStringList live;
    for (auto *screen : QGuiApplication::screens()) {
        const QString n = screen->name().trimmed();
        if (!n.isEmpty() && n != QLatin1String("screen"))
            live << n;
    }
    for (const auto &h : m_wlrManager->heads()) {
        if (!h.name.isEmpty() && !live.contains(h.name))
            live << h.name;
    }
    m_screenNames = live;
    const QStringList names = m_screenNames;

    QStringList existing;
    for (int i = 0; i < m_screenCombo->count(); ++i)
        existing << m_screenCombo->itemText(i);
    if (!names.isEmpty() && existing != names) {
        QString current = m_screenCombo->currentText();
        m_screenCombo->blockSignals(true);
        m_screenCombo->clear();
        for (const auto &n : names)
            m_screenCombo->addItem(n);
        if (!current.isEmpty())
            m_screenCombo->setCurrentText(current);
        m_screenCombo->blockSignals(false);
        if (m_screenCombo->currentIndex() < 0 && m_screenCombo->count() > 0)
            m_screenCombo->setCurrentIndex(0);
    }
    refreshMirrorCombo();

    if (m_resolutionCombo->count() == 0 && m_screenCombo->currentIndex() >= 0)
        onScreenChanged(m_screenCombo->currentIndex());

    refreshUuids();

    // Re-acquire here: disable/enable can rebind the output while wl_output stays the same.
    if (m_screenCombo->currentIndex() >= 0)
        acquireColorControl(m_screenCombo->currentText());
}

void VirtualClient::onGroupAdded(const QString &name)
{
    if (!name.isEmpty() && !m_groupNames.contains(name))
        m_groupNames << name;
}

void VirtualClient::attachGroup(const QString &name)
{
    if (name.isEmpty() || m_groups.contains(name))
        return;

    auto *obj = m_groupManager->getGroup(name);
    if (!obj)
        return;

    auto *group = new OutputGroup(obj);
    group->setParent(this);
    m_groups.insert(name, group);
    connect(group, &OutputGroup::stateChanged, this,
            [this, name](const QString &source, const QStringList &mirrors) {
                onGroupState(name, source, mirrors);
            });
    connect(group, &OutputGroup::removed, this, [this, name]() { m_groups.remove(name); });
    connect(group, &OutputGroup::operationFailed, this, [](uint32_t code, const QString &uuid) {
        qWarning() << "  Output group operation failed:" << code << uuid;
    });
}

void VirtualClient::onGroupsDone()
{
    for (auto it = m_groups.begin(); it != m_groups.end();) {
        if (!m_groupNames.contains(it.key())) {
            it.value()->deleteLater();
            it = m_groups.erase(it);
            continue;
        }
        ++it;
    }

    m_groupNames.removeDuplicates();
    for (const auto &name : std::as_const(m_groupNames))
        attachGroup(name);

    refreshScreenInfo();
}

void VirtualClient::onGroupRemoved(const QString &name)
{
    if (auto *group = m_groups.take(name))
        group->deleteLater();
    m_groupNames.removeAll(name);
    m_mirrorGroups.remove(name);
    updateScreenToGroup();
    refreshScreenInfo();
}

void VirtualClient::onGroupState(const QString &groupName,
                                 const QString &sourceUuid,
                                 const QStringList &mirrorUuids)
{
    if (groupName.isEmpty())
        return;

    QStringList screens;
    const QString source = screenForUuid(sourceUuid);
    if (!source.isEmpty())
        screens << source;
    for (const auto &uuid : std::as_const(mirrorUuids)) {
        const QString screen = screenForUuid(uuid);
        if (!screen.isEmpty())
            screens << screen;
    }

    m_mirrorGroups[groupName] = screens;
    updateScreenToGroup();
    refreshScreenInfo();
}

void VirtualClient::updateScreenToGroup()
{
    m_screenToGroup.clear();
    for (auto it = m_mirrorGroups.begin(); it != m_mirrorGroups.end(); ++it) {
        for (const auto &screen : it.value())
            m_screenToGroup.insert(screen, it.key());
    }
}

void VirtualClient::refreshMirrorCombo()
{
    QString sel = m_screenCombo->currentText();
    const QString prev = m_mirrorCombo->currentData().toString();

    QStringList items;
    QList<QVariant> datas;
    items << "None (no mirror)";
    datas << QVariant();
    for (int i = 0; i < m_screenCombo->count(); ++i) {
        const QString name = m_screenCombo->itemText(i);
        if (name != sel) {
            items << name;
            datas << name;
        }
    }

    if (m_mirrorCombo->count() == items.size()) {
        bool same = true;
        for (int i = 0; i < items.size(); ++i) {
            if (m_mirrorCombo->itemText(i) != items.at(i)
                || m_mirrorCombo->itemData(i).toString() != datas.at(i).toString()) {
                same = false;
                break;
            }
        }
        if (same) {
            const int idx = m_mirrorCombo->findData(prev);
            const int want = idx >= 0 ? idx : 0;
            if (m_mirrorCombo->currentIndex() != want)
                m_mirrorCombo->setCurrentIndex(want);
            return;
        }
    }

    m_mirrorCombo->blockSignals(true);
    m_mirrorCombo->clear();
    m_mirrorCombo->addItem("None (no mirror)", QString());
    for (int i = 1; i < items.size(); ++i)
        m_mirrorCombo->addItem(items.at(i), datas.at(i));
    const int idx = m_mirrorCombo->findData(prev);
    m_mirrorCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_mirrorCombo->blockSignals(false);
}

void VirtualClient::onScreenChanged(int index)
{
    if (index < 0)
        return;
    const QString name = m_screenCombo->itemText(index);

    m_updatingUi = true;

    auto wlrHeads = m_wlrManager->heads();
    for (const auto &h : wlrHeads) {
        if (h.name == name) {
            m_posX->setValue(h.position.x());
            m_posY->setValue(h.position.y());
            m_disableBtn->setText(h.enabled ? "Disable" : "Enable");
            m_resolutionCombo->blockSignals(true);
            m_resolutionCombo->clear();
            int preferredIdx = 0;
            int currentIdx = 0;
            QSize currentModeSize;
            if (h.currentMode >= 0 && h.currentMode < h.modes.size())
                currentModeSize = h.modes[h.currentMode].size;
            QSet<QSize> seenSizes;
            for (int i = 0; i < h.modes.size(); ++i) {
                const auto &mode = h.modes[i];
                if (seenSizes.contains(mode.size))
                    continue;
                seenSizes.insert(mode.size);
                QString label = QString("%1x%2").arg(mode.size.width()).arg(mode.size.height());
                if (mode.preferred) {
                    label += " (preferred)";
                    preferredIdx = m_resolutionCombo->count();
                }
                if (mode.size == currentModeSize)
                    currentIdx = m_resolutionCombo->count();
                m_resolutionCombo->addItem(label, i);
            }
            m_resolutionCombo->setCurrentIndex(h.currentMode >= 0 ? currentIdx : preferredIdx);
            m_resolutionCombo->blockSignals(false);

            onResolutionChanged(m_resolutionCombo->currentIndex());

            m_rotationCombo->setCurrentIndex(h.transform);

            int scaleIdx = m_scaleCombo->findText(QString::number(h.scale, 'f', 2));
            if (scaleIdx >= 0)
                m_scaleCombo->setCurrentIndex(scaleIdx);

            break;
        }
    }

    acquireColorControl(name);

    refreshMirrorCombo();
    if (m_screenToGroup.contains(name)) {
        QString groupName = m_screenToGroup[name];
        auto outputs = m_mirrorGroups.value(groupName);
        if (outputs.size() >= 2 && outputs.first() != name) {
            m_mirrorCombo->blockSignals(true);
            m_mirrorCombo->setCurrentText(outputs.first());
            m_mirrorCombo->blockSignals(false);
        }
    }
    m_primaryBtn->setEnabled(m_mirrorCombo->currentData().toString().isEmpty());
    updatePrimaryButtonText();
    m_updatingUi = false;
}

void VirtualClient::onResolutionChanged(int resIndex)
{
    QString name = m_screenCombo->currentText();
    m_refreshCombo->blockSignals(true);
    m_refreshCombo->clear();
    if (resIndex < 0 || name.isEmpty()) {
        m_refreshCombo->blockSignals(false);
        return;
    }

    int modeIdx = m_resolutionCombo->currentData().toInt();
    auto wlrHeads = m_wlrManager->heads();
    for (const auto &h : wlrHeads) {
        if (h.name == name && modeIdx >= 0 && modeIdx < h.modes.size()) {
            const auto &selMode = h.modes[modeIdx];
            QList<int> refreshList;
            int currentRefresh = -1;
            QSet<int> seenRefresh;
            for (int i = 0; i < h.modes.size(); ++i) {
                const auto &mode = h.modes[i];
                if (mode.size != selMode.size)
                    continue;
                if (seenRefresh.contains(mode.refresh))
                    continue;
                seenRefresh.insert(mode.refresh);
                m_refreshCombo->addItem(QString::number(mode.refresh / 1000.0, 'f', 2) + "Hz", mode.refresh);
                if (i == h.currentMode)
                    currentRefresh = mode.refresh;
                refreshList.append(mode.refresh);
            }
            if (currentRefresh >= 0) {
                int idx = refreshList.indexOf(currentRefresh);
                if (idx >= 0)
                    m_refreshCombo->setCurrentIndex(idx);
            }
            break;
        }
    }
    m_refreshCombo->blockSignals(false);
}


void VirtualClient::onMirrorChanged(int index)
{
    Q_UNUSED(index);
    updateMirrorGroup();
    m_primaryBtn->setEnabled(m_mirrorCombo->currentData().toString().isEmpty());
    updatePrimaryButtonText();
}

void VirtualClient::onBrightnessChanged(int value)
{
    m_brightnessLabel->setText(QString("%1%").arg(value));
    // Debounce the commit: restarts the single-shot timer on every intermediate
    // value so only the final settled value is pushed to the compositor.
    m_brightnessCommitTimer->start();
}

void VirtualClient::onColorTempChanged(int value)
{
    m_colorTempLabel->setText(QString("%1K").arg(value));
    m_colorTempCommitTimer->start();
}

void VirtualClient::onBrightnessSliderReleased()
{
    // Commit immediately on release and cancel any pending debounce.
    m_brightnessCommitTimer->stop();
    commitBrightness();
}

void VirtualClient::onColorTempSliderReleased()
{
    m_colorTempCommitTimer->stop();
    commitColorTemp();
}

void VirtualClient::commitBrightness()
{
    if (m_colorControl) {
        m_colorControl->set_brightness(wl_fixed_from_double(m_brightnessSlider->value()));
        m_colorControl->commit();
    }
}

void VirtualClient::commitColorTemp()
{
    if (m_colorControl) {
        m_colorControl->set_color_temperature(static_cast<uint32_t>(m_colorTempSlider->value()));
        m_colorControl->commit();
    }
}

void VirtualClient::onColorControlBrightness(int percent)
{
    m_brightnessSlider->blockSignals(true);
    m_brightnessSlider->setValue(percent);
    m_brightnessSlider->blockSignals(false);
    m_brightnessLabel->setText(QString("%1%").arg(percent));
}

void VirtualClient::onColorControlColorTemp(uint32_t kelvin)
{
    m_colorTempSlider->blockSignals(true);
    m_colorTempSlider->setValue(static_cast<int>(kelvin));
    m_colorTempSlider->blockSignals(false);
    m_colorTempLabel->setText(QString("%1K").arg(kelvin));
}

void VirtualClient::acquireColorControl(const QString &screenName)
{
    if (m_colorControl) {
        m_colorControl->destroy();
        delete m_colorControl;
        m_colorControl = nullptr;
    }
    if (!m_treelandManager->isActive())
        return;
    const QString uuid = uuidForScreen(screenName);
    if (uuid.isEmpty())
        return;
    auto *cc = m_treelandManager->getPictureControl(uuid);
    if (!cc)
        return;
    m_colorControl = new ColorControl(cc, this);
    connect(m_colorControl, &ColorControl::brightnessChanged,
            this, &VirtualClient::onColorControlBrightness);
    connect(m_colorControl, &ColorControl::colorTemperatureChanged,
            this, &VirtualClient::onColorControlColorTemp);
}
void VirtualClient::applyCurrentSettings()
{
    if (m_updatingUi)
        return;
    QString name = m_screenCombo->currentText();
    if (name.isEmpty())
        return;

    int x = m_posX->value();
    int y = m_posY->value();

    QSize modeSize;
    int refresh = 0;
    int modeIdx = m_resolutionCombo->currentData().toInt();
    auto wlrHeads = m_wlrManager->heads();
    for (const auto &h : wlrHeads) {
        if (h.name == name) {
            if (modeIdx >= 0 && modeIdx < h.modes.size()) {
                modeSize = h.modes[modeIdx].size;
                refresh = m_refreshCombo->currentData().toInt();
            }
            break;
        }
    }

    int transform = m_rotationCombo->currentData().toInt();
    double scale = m_scaleCombo->currentData().toDouble();

    m_wlrManager->applyConfig(name, true, QPoint(x, y), modeSize, refresh, transform, scale);
}

void VirtualClient::onDisableClicked()
{
    QString name = m_screenCombo->currentText();
    if (name.isEmpty())
        return;

    bool enabled = false;
    const auto wlrHeads = m_wlrManager->heads();
    for (const auto &h : wlrHeads) {
        if (h.name == name) {
            enabled = h.enabled;
            break;
        }
    }

    if (enabled) {
        m_wlrManager->applyConfig(name, false, QPoint(), QSize(), 0, 0, 1.0);
        m_disableBtn->setText("Enable");
    } else {
        applyCurrentSettings();
        m_disableBtn->setText("Disable");
    }
}

void VirtualClient::onPrimaryClicked()
{
    QString name = m_screenCombo->currentText();
    if (name.isEmpty())
        return;
    if (!m_treelandManager->isActive())
        return;
    const QString uuid = uuidForScreen(name);
    if (uuid.isEmpty()) {
        qWarning() << "No uuid known for" << name << "yet";
        return;
    }
    m_treelandManager->set_primary_output(uuid);
}

void VirtualClient::onPrimaryOutputChanged(const QString &uuid)
{
    m_primaryUuid = uuid;
    updatePrimaryLabel();
    updatePrimaryButtonText();
}

void VirtualClient::updatePrimaryLabel()
{
    if (!m_primaryLabel)
        return;
    const QString name = screenForUuid(m_primaryUuid);
    m_primaryLabel->setText(QString("Primary: %1").arg(name.isEmpty() ? QStringLiteral("—") : name));
}

void VirtualClient::updatePrimaryButtonText()
{
    if (m_primaryBtn) {
        const QString current = m_screenCombo ? m_screenCombo->currentText() : QString();
        const bool isPrimary = !m_primaryUuid.isEmpty() && !current.isEmpty()
            && uuidForScreen(current) == m_primaryUuid;
        m_primaryBtn->setText(isPrimary ? "Primary ✓" : "Primary");
    }
}
void VirtualClient::onWlrHeadsChanged()
{
    // Groups are pushed through group_added/group_removed; the output group
    // protocol has no list request.
    refreshScreenInfo();
}

void VirtualClient::onWlrApplyFinished(bool ok)
{
    Q_UNUSED(ok);

    refreshScreenInfo();
}

void VirtualClient::updateMirrorGroup()
{
    const QString screen = m_screenCombo->currentText();
    const QString mirror = m_mirrorCombo->currentData().toString();

    if (screen.isEmpty())
        return;

    // Leaving a group is explicit: dissolve the group the screen belongs to.
    if (m_screenToGroup.contains(screen)) {
        const QString oldGroup = m_screenToGroup[screen];
        if (auto *group = m_groups.take(oldGroup)) {
            group->dissolve();
            group->deleteLater();
        }
        m_groupNames.removeAll(oldGroup);
        m_mirrorGroups.remove(oldGroup);
        updateScreenToGroup();
    }

    if (!mirror.isEmpty()) {
        const QString sourceUuid = uuidForScreen(screen);
        const QString mirrorUuid = uuidForScreen(mirror);
        if (sourceUuid.isEmpty() || mirrorUuid.isEmpty()) {
            // The uuid arrives asynchronously; retry once it is known.
            m_pendingMirrorScreen = mirror;
            refreshUuids();
            refreshScreenInfo();
            return;
        }
        m_pendingMirrorScreen.clear();

        // The compositor assigns the group name; the state of the group is
        // reported as soon as it exists, so the mirror is queued right away.
        auto *group = new OutputGroup(m_groupManager->createGroup(sourceUuid));
        group->setParent(this);
        connect(group, &OutputGroup::nameAssigned, this, [this, group](const QString &name) {
            m_groupNames << name;
            m_groups.insert(name, group);
        });
        connect(group, &OutputGroup::stateChanged, this,
                [this, group](const QString &source, const QStringList &mirrors) {
                    onGroupState(group->name(), source, mirrors);
                });
        connect(group, &OutputGroup::removed, this, [this, group]() {
            if (!group->name().isEmpty())
                m_groups.remove(group->name());
        });
        connect(group, &OutputGroup::operationFailed, this, [](uint32_t code, const QString &uuid) {
            qWarning() << "  Output group operation failed:" << code << uuid;
        });
        group->requestAddMirror(mirrorUuid);
        group->commit();
    }

    refreshScreenInfo();
}

#include "virtualclient.moc"

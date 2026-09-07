// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualclient.h"
#include "virtualoutput.h"
#include "virtualoutputmanager.h"
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
#include <QVBoxLayout>
#include <QWidget>
#include <QtWaylandClient/QWaylandClientExtension>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandscreen_p.h>

#include <wayland-client-core.h>
#include <cmath>

// ---------------------------------------------------------------------------
// TreelandOutputManager
// ---------------------------------------------------------------------------

TreelandOutputManager::TreelandOutputManager()
    : QWaylandClientExtensionTemplate<TreelandOutputManager>(2)
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

struct ::treeland_output_color_control_v1 *TreelandOutputManager::getColorControl(struct ::wl_output *output)
{
    if (!isInitialized())
        return nullptr;
    return get_color_control(output);
}

// ---------------------------------------------------------------------------
// ColorControl — wraps treeland_output_color_control_v1 and emits on events
// ---------------------------------------------------------------------------

class ColorControl : public QObject, public QtWayland::treeland_output_color_control_v1
{
    Q_OBJECT
public:
    ColorControl(struct ::treeland_output_color_control_v1 *obj, QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_output_color_control_v1(obj)
    {
    }
    // Generated wrappers do not destroy their Wayland resource.
    ~ColorControl() override
    {
        if (isInitialized())
            destroy();
    }

Q_SIGNALS:
    void brightnessChanged(int percent);
    void colorTemperatureChanged(uint32_t kelvin);

protected:
    void treeland_output_color_control_v1_brightness(wl_fixed_t b) override
    {
        Q_EMIT brightnessChanged(static_cast<int>(std::round(wl_fixed_to_double(b))));
    }
    void treeland_output_color_control_v1_color_temperature(uint32_t t) override
    {
        Q_EMIT colorTemperatureChanged(t);
    }
    void treeland_output_color_control_v1_result(uint32_t) override {}
};

// ---------------------------------------------------------------------------
// VirtualClient
// ---------------------------------------------------------------------------
VirtualClient::VirtualClient(QObject *parent)
    : QObject(parent)
{
    m_virtualManager = std::make_unique<VirtualOutputManager>();
    m_treelandManager = std::make_unique<TreelandOutputManager>();
    m_wlrManager = std::make_unique<WlrOutputManager>();

    // Bind managers
    connect(m_virtualManager.get(), &VirtualOutputManager::activeChanged, this, [this]() {
        if (m_virtualManager->isActive())
            setupUi();
    });
    connect(m_virtualManager.get(), &VirtualOutputManager::virtualOutputListReceived,
            this, &VirtualClient::onVirtualOutputListReceived);
    connect(m_virtualManager.get(), &VirtualOutputManager::virtualOutputModified,
            this, &VirtualClient::onVirtualOutputModified);

    connect(m_treelandManager.get(), &TreelandOutputManager::primaryOutputChanged,
            this, &VirtualClient::onPrimaryOutputChanged);

    connect(m_wlrManager.get(), &WlrOutputManager::headsChanged, this, &VirtualClient::onWlrHeadsChanged);
    connect(m_wlrManager.get(), &WlrOutputManager::applyFinished, this, &VirtualClient::onWlrApplyFinished);

    // Trigger initialization
    m_virtualManager->instantiate();
    m_treelandManager->instantiate();
    m_wlrManager->instantiate();
}

VirtualClient::~VirtualClient()
{
    delete m_colorControl;
}

void VirtualClient::setupUi()
{
    // Guard against being called twice (e.g. if the extension re-activates)
    if (m_widget)
        return;

    m_widget = new QWidget;
    m_widget->resize(640, 700);

    auto *mainLayout = new QVBoxLayout(m_widget);
    mainLayout->setSpacing(8);

    // --- Screen info label ---
    m_infoLabel = new QLabel("Loading screen info...");
    m_infoLabel->setWordWrap(true);
    mainLayout->addWidget(m_infoLabel);

    // --- Screen selection + mirror row ---
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

    // --- Settings group ---
    auto *settingsGroup = new QGroupBox("Screen Settings");
    auto *form = new QFormLayout(settingsGroup);
    form->setSpacing(6);

    // Brightness
    m_brightnessSlider = new QSlider(Qt::Horizontal);
    m_brightnessSlider->setRange(0, 100);
    m_brightnessSlider->setValue(100);
    m_brightnessLabel = new QLabel("100%");
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &VirtualClient::onBrightnessChanged);
    auto *brightRow = new QHBoxLayout;
    brightRow->addWidget(m_brightnessSlider);
    brightRow->addWidget(m_brightnessLabel);
    form->addRow("Brightness:", brightRow);

    // Color temperature
    m_colorTempSlider = new QSlider(Qt::Horizontal);
    m_colorTempSlider->setRange(1000, 20000);
    m_colorTempSlider->setValue(6500);
    m_colorTempLabel = new QLabel("6500K");
    connect(m_colorTempSlider, &QSlider::valueChanged, this, &VirtualClient::onColorTempChanged);
    auto *ctRow = new QHBoxLayout;
    ctRow->addWidget(m_colorTempSlider);
    ctRow->addWidget(m_colorTempLabel);
    form->addRow("Color Temp:", ctRow);

    // Position
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

    // Resolution
    m_resolutionCombo = new QComboBox;
    form->addRow("Resolution:", m_resolutionCombo);

    // Refresh rate
    m_refreshCombo = new QComboBox;
    form->addRow("Refresh:", m_refreshCombo);

    // Rotation
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

    // Scale
    m_scaleCombo = new QComboBox;
    for (double s : {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0})
        m_scaleCombo->addItem(QString::number(s, 'f', 2), s);
    m_scaleCombo->setCurrentText("1.00");
    form->addRow("Scale:", m_scaleCombo);

    mainLayout->addWidget(settingsGroup);

    mainLayout->addStretch();

    // Connect UI signals
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

    // Fetch existing mirror groups
    m_virtualManager->getVirtualOutputList();

    // Initial screen info refresh
    refreshScreenInfo();
}

struct ::wl_output *VirtualClient::getWlOutput(const QString &name)
{
    QWindow *window = m_widget ? m_widget->windowHandle() : nullptr;
    if (!window || !window->handle())
        return nullptr;
    auto *waylandWindow = static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
    auto screens = waylandWindow->display()->screens();
    for (auto *screen : screens) {
        if (screen->name() == name)
            return screen->output();
    }
    return nullptr;
}

void VirtualClient::refreshScreenInfo()
{
    if (!m_widget)
        return;

    // Build screen info text
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

    // Wlr head info
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

    // Mirror groups
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

    // Keep disabled heads but remove unplugged outputs.
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
    // Refresh mirror combo
    refreshMirrorCombo();

    if (m_resolutionCombo->count() == 0 && m_screenCombo->currentIndex() >= 0)
        onScreenChanged(m_screenCombo->currentIndex());

    // Mirror changes replace wl_output proxies.
    if (m_screenCombo->currentIndex() >= 0) {
        const QString cur = m_screenCombo->currentText();
        auto *wlOut = getWlOutput(cur);
        if (wlOut != m_colorOutput)
            acquireColorControl(cur);
    }
}

void VirtualClient::onVirtualOutputModified(const QString &name)
{
    Q_UNUSED(name);
    m_virtualManager->getVirtualOutputList();
}

void VirtualClient::refreshMirrorCombo()
{
    QString sel = m_screenCombo->currentText();
    // Preserve the previously selected mirror (by its screen name) so that
    // refreshing the combo after a mirror change does not reset it to "None".
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

    // Avoid closing an open popup when choices are unchanged.
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

    // Get wlr head info for this screen
    auto wlrHeads = m_wlrManager->heads();
    for (const auto &h : wlrHeads) {
        if (h.name == name) {
            // Update position spinboxes
            m_posX->setValue(h.position.x());
            m_posY->setValue(h.position.y());
            m_disableBtn->setText(h.enabled ? "Disable" : "Enable");
            // Update resolution combo
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
            // Set to current or preferred mode
            m_resolutionCombo->setCurrentIndex(h.currentMode >= 0 ? currentIdx : preferredIdx);
            m_resolutionCombo->blockSignals(false);

            // Refresh rate: update when resolution changes
            onResolutionChanged(m_resolutionCombo->currentIndex());

            // Rotation
            m_rotationCombo->setCurrentIndex(h.transform);

            // Scale
            int scaleIdx = m_scaleCombo->findText(QString::number(h.scale, 'f', 2));
            if (scaleIdx >= 0)
                m_scaleCombo->setCurrentIndex(scaleIdx);

            break;
        }
    }

    // Acquire color control for this screen (treeland output manager)
    acquireColorControl(name);

    // Update mirror combo to reflect whether this screen is in a mirror group
    refreshMirrorCombo(); // already blocks+unblocks its own signals
    if (m_screenToGroup.contains(name)) {
        QString groupName = m_screenToGroup[name];
        auto outputs = m_mirrorGroups.value(groupName);
        if (outputs.size() >= 2 && outputs.first() != name) {
            // This screen mirrors another one; select that target.
            m_mirrorCombo->blockSignals(true);
            m_mirrorCombo->setCurrentText(outputs.first());
            m_mirrorCombo->blockSignals(false);
        }
    }
    m_primaryBtn->setEnabled(m_mirrorCombo->currentData().toString().isEmpty());
    updatePrimaryButtonText();
    m_updatingUi = false;
}

// Called when the resolution combo changes; fills the refresh-rate combo with
// the refresh rates of the selected resolution.
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
            // collect refresh rates for this resolution
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
    // A mirror screen cannot be the primary; update the button state too.
    m_primaryBtn->setEnabled(m_mirrorCombo->currentData().toString().isEmpty());
    updatePrimaryButtonText();
}

void VirtualClient::onBrightnessChanged(int value)
{
    m_brightnessLabel->setText(QString("%1%").arg(value));
    if (m_colorControl) {
        m_colorControl->set_brightness(wl_fixed_from_double(value));
        m_colorControl->commit();
    }
}

void VirtualClient::onColorTempChanged(int value)
{
    m_colorTempLabel->setText(QString("%1K").arg(value));
    if (m_colorControl) {
        m_colorControl->set_color_temperature(static_cast<uint32_t>(value));
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
    // Destroy the resource before deleting its generated wrapper.
    if (m_colorControl) {
        m_colorControl->destroy();
        delete m_colorControl;
        m_colorControl = nullptr;
    }
    m_colorOutput = nullptr;
    if (!m_treelandManager->isActive())
        return;
    auto *wlOut = getWlOutput(screenName);
    if (!wlOut)
        return;
    auto *cc = m_treelandManager->getColorControl(wlOut);
    if (!cc)
        return;
    m_colorOutput = wlOut;
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

    // Position
    int x = m_posX->value();
    int y = m_posY->value();

    // Resolution: find the selected mode
    QSize modeSize;
    int refresh = 0;
    int modeIdx = m_resolutionCombo->currentData().toInt();
    auto wlrHeads = m_wlrManager->heads();
    for (const auto &h : wlrHeads) {
        if (h.name == name) {
            if (modeIdx >= 0 && modeIdx < h.modes.size()) {
                modeSize = h.modes[modeIdx].size;
                // Preserve the exact server-provided mHz value.
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

    // Find this head to decide whether it is currently enabled.
    bool enabled = false;
    const auto wlrHeads = m_wlrManager->heads();
    for (const auto &h : wlrHeads) {
        if (h.name == name) {
            enabled = h.enabled;
            break;
        }
    }

    if (enabled) {
        // Disable the current screen (other params are ignored when disabled).
        m_wlrManager->applyConfig(name, false, QPoint(), QSize(), 0, 0, 1.0);
        m_disableBtn->setText("Enable");
    } else {
        // Re-enable with the current in-memory settings.
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
    m_treelandManager->set_primary_output(name);
}

void VirtualClient::onPrimaryOutputChanged(const QString &name)
{
    m_primaryName = name;
    if (m_primaryLabel) {
        m_primaryLabel->setText(QString("Primary: %1").arg(name.isEmpty() ? QStringLiteral("—") : name));
    }
    updatePrimaryButtonText();
}

void VirtualClient::updatePrimaryButtonText()
{
    if (m_primaryBtn) {
        const bool isPrimary = !m_primaryName.isEmpty()
            && m_screenCombo && m_screenCombo->currentText() == m_primaryName;
        m_primaryBtn->setText(isPrimary ? "Primary ✓" : "Primary");
    }
}
void VirtualClient::onWlrHeadsChanged()
{
    m_virtualManager->getVirtualOutputList();
    refreshScreenInfo();
}

void VirtualClient::onWlrApplyFinished(bool ok)
{
    Q_UNUSED(ok);

    // Refresh screen info to reflect changes
    refreshScreenInfo();
}

void VirtualClient::onVirtualOutputListReceived(const QStringList &names)
{
    // Do not destroy stale server-side groups again.
    for (auto it = m_virtualOutputs.begin(); it != m_virtualOutputs.end();) {
        if (names.contains(it.key())) {
            ++it;
            continue;
        }
        qInfo() << "  Dropping group no longer on server:" << it.key();
        it = m_virtualOutputs.erase(it);
    }
    m_screenToGroup.clear();
    for (auto it = m_mirrorGroups.begin(); it != m_mirrorGroups.end();) {
        if (!names.contains(it.key())) {
            it = m_mirrorGroups.erase(it);
            continue;
        }
        for (const auto &screen : it.value())
            m_screenToGroup.insert(screen, it.key());
        ++it;
    }

    // Bind the newly discovered groups, if any.
    for (const auto &name : names) {
        if (m_virtualOutputs.contains(name)) {
            continue;
        }
        auto *obj = m_virtualManager->getVirtualOutput(name);
        if (!obj) {
            qWarning() << "  Failed to get virtual output for:" << name;
            continue;
        }
        auto *vo = new VirtualOutput(obj);
        m_virtualOutputs.insert(name, QSharedPointer<VirtualOutput>(vo));
        connect(vo, &VirtualOutput::outputsReceived, this, &VirtualClient::onVirtualOutputOutputs);
    }

    refreshScreenInfo();
}

void VirtualClient::onVirtualOutputOutputs(const QString &name, const QStringList &outputs)
{
    m_mirrorGroups[name] = outputs;
    m_screenToGroup.clear();
    for (auto it = m_mirrorGroups.begin(); it != m_mirrorGroups.end(); ++it) {
        for (const auto &screen : it.value()) {
            m_screenToGroup.insert(screen, it.key());
        }
    }
    refreshScreenInfo();
}

void VirtualClient::updateMirrorGroup()
{
    QString screen = m_screenCombo->currentText();
    QString mirror = m_mirrorCombo->currentData().toString(); // empty = "None"

    if (screen.isEmpty())
        return;

    // Remove screen from any existing mirror group
    if (m_screenToGroup.contains(screen)) {
        QString oldGroup = m_screenToGroup[screen];
        if (auto existing = m_virtualOutputs.value(oldGroup)) {
            existing->destroy();
        }
        m_virtualOutputs.remove(oldGroup);
        m_mirrorGroups.remove(oldGroup);
        m_screenToGroup.clear();
        // Rebuild map
        for (auto it = m_mirrorGroups.begin(); it != m_mirrorGroups.end(); ++it) {
            for (const auto &s : it.value())
                m_screenToGroup.insert(s, it.key());
        }
    }

    // If mirror target is set, create a new group
    if (!mirror.isEmpty()) {
        QByteArray screenNameArray;
        screenNameArray.append(mirror.toUtf8());
        screenNameArray.append('\0');
        screenNameArray.append(screen.toUtf8());
        screenNameArray.append('\0');
        screenNameArray.append('\0');

        QString groupName = QString("copyscreen_%1").arg(screen);
        auto *obj = m_virtualManager->createVirtualOutput(groupName, screenNameArray);
        if (obj) {
            auto *vo = new VirtualOutput(obj);
            m_virtualOutputs.insert(groupName, QSharedPointer<VirtualOutput>(vo));
            connect(vo, &VirtualOutput::outputsReceived, this, &VirtualClient::onVirtualOutputOutputs);
            m_mirrorGroups[groupName] = {mirror, screen};
            m_screenToGroup[screen] = groupName;
            m_screenToGroup[mirror] = groupName;
        }
    }

    refreshScreenInfo();
}

// moc for ColorControl (defined in this file)
#include "virtualclient.moc"


// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "seatsmanager.h"
#include "common/treelandlogging.h"
#include "input/inputdevice.h"

#include <wbackend.h>
#include <wcursor.h>
#include <woutputlayout.h>
#include <qwinputdevice.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QInputEvent>
#include <QQuickWindow>

SeatsManager::SeatsManager(WServer *server, QObject *parent)
    : QObject(parent)
    , m_server(server)
{
}

SeatsManager::~SeatsManager()
{
    QMap<QString, WSeat*> seatsToDelete;
    seatsToDelete.swap(m_seats);
    m_deviceRules.clear();
    m_defaultSeat = nullptr;

    for (auto seat : std::as_const(seatsToDelete)) {
        if (m_server) {
            m_server->detach(seat);
        }
        delete seat;
    }
}

WSeat *SeatsManager::createSeat(const QString &name, bool isFallback)
{
    if (m_seats.contains(name)) {
        qCDebug(treelandSeat) << "Seat" << name << "already exists";
        return m_seats[name];
    }

    WSeat *seat = new WSeat(name);
    m_seats[name] = seat;

    if (isFallback) {
        m_defaultSeat = seat;
    }


    Q_EMIT seatAdded(seat);
    qCDebug(treelandSeat) << "Created seat:" << name << "fallback:" << isFallback;
    
    return seat;
}

void SeatsManager::removeSeat(const QString &name)
{
    if (!m_seats.contains(name)) {
        qCWarning(treelandSeat) << "Cannot remove non-existent seat:" << name;
        return;
    }

    WSeat *seat = m_seats.take(name);

    if (m_defaultSeat == seat && !m_seats.isEmpty()) {
        m_defaultSeat = m_seats.begin().value();
        qCDebug(treelandSeat) << "Fallback seat changed to:" << m_defaultSeat->name();
    } else if (m_defaultSeat == seat) {
        m_defaultSeat = nullptr;
        qCDebug(treelandSeat) << "No fallback seat available";
    }

    QList<WInputDevice*> devices = seat->deviceList();
    for (auto device : std::as_const(devices)) {
        seat->detachInputDevice(device);

        clearDeviceCache(device);
        // Reassign device to appropriate seat
        if (!m_seats.isEmpty()) {
            WSeat *newSeat = autoAssignDevice(device);
            if (newSeat) {
                Q_EMIT deviceReassigned(device, seat, newSeat);
            } else {
                qCWarning(treelandSeat) << "Failed to reassign device after seat removal:" << device->name();
            }
        }
    }
    
    if (m_server) {
        m_server->detach(seat);
    }

    Q_EMIT seatRemoved(seat);
    qCDebug(treelandSeat) << "Removed seat:" << name;

    delete seat;

    // Clean up device rules for removed seat
    m_deviceRules.remove(name);
}

void SeatsManager::removeSeat(WSeat *seat)
{
    Q_ASSERT(seat);

    QString seatName;
    for (auto it = m_seats.begin(); it != m_seats.end(); ++it) {
        if (it.value() == seat) {
            seatName = it.key();
            break;
        }
    }

    if (!seatName.isEmpty()) {
        removeSeat(seatName);
    } else {
        qCWarning(treelandSeat) << "Attempted to remove a seat that is not managed by SeatsManager";
    }
}

WSeat *SeatsManager::getSeat(const QString &name) const
{
    return m_seats.value(name);
}

QList<WSeat*> SeatsManager::seats() const
{
    return m_seats.values();
}

WSeat *SeatsManager::fallbackSeat() const
{
    return m_defaultSeat;
}

void SeatsManager::assignDeviceToSeat(WInputDevice *device, const QString &seatName)
{
    if (!device) {
        qCWarning(treelandSeat) << "Cannot assign null device to seat";
        return;
    }

    for (auto seat : std::as_const(m_seats)) {
        if (seat->deviceList().contains(device)) {
            if (seat->name() == seatName) {
                qCDebug(treelandSeat) << "Device" << device->name() << "already assigned to seat" << seatName;
                return;
            }

            seat->detachInputDevice(device);
            qCDebug(treelandSeat) << "Device" << device->name() << "detached from seat" << seat->name();
            break;
        }
    }

    if (m_seats.contains(seatName)) {
        m_seats[seatName]->attachInputDevice(device);
        qCDebug(treelandSeat) << "Device" << device->name() << "assigned to seat" << seatName;
    } else if (fallbackSeat()) {
        fallbackSeat()->attachInputDevice(device);
        qCDebug(treelandSeat) << "Device" << device->name() << "assigned to fallback seat";
    } else {
        qCWarning(treelandSeat) << "Cannot assign device" << device->name() << "- no seats available";
    }
}

WSeat *SeatsManager::autoAssignDevice(WInputDevice *device)
{
    if (!device) {
        qCWarning(treelandSeat) << "Cannot auto-assign null device";
        return nullptr;
    }

    for (auto seat : std::as_const(m_seats)) {
        if (seat->deviceList().contains(device)) {
            qCDebug(treelandSeat) << "Device" << device->name() << "already assigned to seat" << seat->name();
            return seat;
        }
    }

    WSeat *targetSeat = findSeatForDevice(device);

    if (targetSeat) {
        targetSeat->attachInputDevice(device);
        qCDebug(treelandSeat) << "Device" << device->name() << "auto-assigned to seat" << targetSeat->name();
        return targetSeat;
    } else if (fallbackSeat()) {
        fallbackSeat()->attachInputDevice(device);
        qCDebug(treelandSeat) << "Device" << device->name() << "auto-assigned to fallback seat";
        return fallbackSeat();
    }

    qCWarning(treelandSeat) << "Failed to auto-assign device" << device->name();
    return nullptr;
}

void SeatsManager::addDeviceRule(const QString &seatName, const QString &rule)
{
    if (seatName.isEmpty()) {
        qCWarning(treelandSeat) << "Cannot add device rule for seat with empty name";
        return;
    }

    if (rule.isEmpty()) {
        qCWarning(treelandSeat) << "Cannot add empty device rule";
        return;
    }

    if (!m_seats.contains(seatName)) {
        qCWarning(treelandSeat) << "Cannot add device rule for non-existent seat:" << seatName;
        return;
    }

    QRegularExpression regex(rule);
    if (!regex.isValid()) {
        qCWarning(treelandSeat) << "Invalid regex pattern for device rule:" << rule << "Error:" << regex.errorString();
        return;
    }

    if (!m_deviceRules.contains(seatName)) {
        m_deviceRules[seatName] = QList<QRegularExpression>();
    }

    // Check for duplicate rules
    for (const auto &existingRule : std::as_const(m_deviceRules[seatName])) {
        if (existingRule.pattern() == rule) {
            qCDebug(treelandSeat) << "Device rule already exists for seat" << seatName << ":" << rule;
            return;
        }
    }

    m_deviceRules[seatName].append(regex);
    qCDebug(treelandSeat) << "Added device rule for seat" << seatName << ":" << rule;
}

void SeatsManager::removeDeviceRule(const QString &seatName, const QString &rule)
{
    if (!m_deviceRules.contains(seatName)) {
        qCDebug(treelandSeat) << "No device rules for seat:" << seatName;
        return;
    }

    QRegularExpression regex(rule);
    if (!regex.isValid()) {
        qCWarning(treelandSeat) << "Invalid regex pattern:" << rule;
        return;
    }

    auto &rules = m_deviceRules[seatName];
    for (int i = 0; i < rules.size(); i++) {
        if (rules[i].pattern() == regex.pattern()) {
            rules.removeAt(i);
            qCDebug(treelandSeat) << "Removed device rule from seat" << seatName << ":" << rule;
            break;
        }
    }

    if (rules.isEmpty()) {
        m_deviceRules.remove(seatName);
        qCDebug(treelandSeat) << "All device rules removed for seat:" << seatName;
    }
}

QStringList SeatsManager::deviceRules(const QString &seatName) const
{
    if (!m_deviceRules.contains(seatName))
        return QStringList();

    QStringList result;
    const auto &rules = m_deviceRules[seatName];
    for (const auto &regex : std::as_const(rules)) {
        result.append(regex.pattern());
    }

    return result;
}

void SeatsManager::loadConfig(const QJsonObject &config)
{
    qCDebug(treelandSeat) << "Loading seat configuration";

    QMap<QString, WSeat*> seatsToDelete;
    seatsToDelete.swap(m_seats);
    m_deviceRules.clear();
    m_defaultSeat = nullptr;

    for (auto seat : std::as_const(seatsToDelete)) {
        if (m_server) {
            m_server->detach(seat);
        }
        Q_EMIT seatRemoved(seat);
        delete seat;
    }

    QJsonArray seatsArray = config["seats"].toArray();
    for (const auto &seatValue : std::as_const(seatsArray)) {
        QJsonObject seatObj = seatValue.toObject();
        QString name = seatObj["name"].toString();
        bool isFallback = seatObj["fallback"].toBool();

        createSeat(name, isFallback);

        QJsonArray rulesArray = seatObj["deviceRules"].toArray();
        for (const auto &ruleValue : std::as_const(rulesArray)) {
            QString rule = ruleValue.toString();
            addDeviceRule(name, rule);
        }
    }

    if (m_seats.isEmpty()) {
        qCDebug(treelandSeat) << "No seats in config, creating default seat0";
        createSeat("seat0", true);
    }

    if (!m_defaultSeat && !m_seats.isEmpty()) {
        m_defaultSeat = m_seats.begin().value();
        qCDebug(treelandSeat) << "Set fallback seat to:" << m_defaultSeat->name();
    }

    qCDebug(treelandSeat) << "Loaded" << m_seats.size() << "seats";
}

bool SeatsManager::loadConfigFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCDebug(treelandSeat) << "Config file not found:" << filePath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(treelandSeat) << "Invalid JSON in config file:" << filePath;
        return false;
    }

    QJsonObject config = doc.object();
    if (!validateConfig(config)) {
        qCWarning(treelandSeat) << "Invalid seat configuration in:" << filePath;
        return false;
    }

    loadConfig(config);
    return true;
}

bool SeatsManager::saveConfigToFile(const QString &filePath) const
{
    QJsonObject config = saveConfig();
    QJsonDocument doc(config);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(treelandSeat) << "Cannot write config file:" << filePath;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    qCDebug(treelandSeat) << "Saved configuration to:" << filePath;
    return true;
}

bool SeatsManager::validateConfig(const QJsonObject &config) const
{
    if (!config.contains("seats") || !config["seats"].isArray()) {
        return false;
    }

    QJsonArray seatsArray = config["seats"].toArray();
    bool hasFallback = false;

    for (const auto &seatValue : seatsArray) {
        if (!seatValue.isObject()) {
            return false;
        }

        QJsonObject seatObj = seatValue.toObject();
        if (!seatObj.contains("name") || !seatObj["name"].isString()) {
            return false;
        }

        if (seatObj.contains("fallback") && seatObj["fallback"].toBool()) {
            if (hasFallback) {
                qCWarning(treelandSeat) << "Multiple fallback seats found in configuration";
                return false;
            }
            hasFallback = true;
        }

        if (seatObj.contains("deviceRules") && !seatObj["deviceRules"].isArray()) {
            return false;
        }
    }

    return true;
}

void SeatsManager::initializeDefaultSeat()
{
    if (!m_seats.isEmpty()) {
        qCDebug(treelandSeat) << "Seats already initialized";
        return;
    }

    WSeat *defaultSeat = createSeat("seat0", true);
    if (!defaultSeat) {
        qCCritical(treelandSeat) << "Failed to create default seat!";
        return;
    }

    ensureDefaultDeviceRules("seat0");

    qCDebug(treelandSeat) << "Initialized default seat: seat0";
}

void SeatsManager::ensureDefaultDeviceRules(const QString &seatName)
{
    if (deviceRules(seatName).isEmpty()) {
        addDeviceRule(seatName, QString("%1:.*").arg(static_cast<int>(WInputDevice::Type::Keyboard)));
        addDeviceRule(seatName, QString("%1:.*").arg(static_cast<int>(WInputDevice::Type::Pointer)));
        addDeviceRule(seatName, QString("%1:.*").arg(static_cast<int>(WInputDevice::Type::Touch)));
        qCDebug(treelandSeat) << "Added default device rules for seat:" << seatName;
    }
}

WSeat *SeatsManager::initializeFromConfig(const QString &configPath, WServer *server)
{
    // Try to load config from file
    if (!loadConfigFromFile(configPath)) {
        qCDebug(treelandSeat) << "No valid seat config found at:" << configPath;
    }

    // Ensure at least one seat exists
    if (m_seats.isEmpty()) {
        initializeDefaultSeat();
    }

    // Determine primary seat
    WSeat *primarySeat = fallbackSeat();
    if (!primarySeat && !m_seats.isEmpty()) {
        primarySeat = m_seats.first();
    }

    if (!primarySeat) {
        qCCritical(treelandSeat) << "No seat available after initialization!";
        return nullptr;
    }

    // Ensure primary seat has default device rules
    ensureDefaultDeviceRules(primarySeat->name());

    // Attach all seats to server
    if (server && server->isRunning()) {
        for (auto *seat : std::as_const(m_seats)) {
            if (seat->server() != server) {
                server->attach(seat);
            }
        }
    }

    qCDebug(treelandSeat) << "Seat initialization complete. Primary seat:" << primarySeat->name();
    return primarySeat;
}

QJsonObject SeatsManager::saveConfig() const
{
    QJsonObject config;
    QJsonArray seatsArray;
    
    for (auto it = m_seats.begin(); it != m_seats.end(); ++it) {
        QString name = it.key();
        WSeat *seat = it.value();
        
        QJsonObject seatObj;
        seatObj["name"] = name;
        seatObj["fallback"] = (seat == m_defaultSeat);
        
        QJsonArray rulesArray;
        const auto rules = deviceRules(name);
        for (const auto &rule : std::as_const(rules)) {
            rulesArray.append(rule);
        }
        seatObj["deviceRules"] = rulesArray;
        
        seatsArray.append(seatObj);
    }
    
    config["seats"] = seatsArray;
    qCDebug(treelandSeat) << "Saved configuration for" << m_seats.size() << "seats";
    return config;
}

bool SeatsManager::matchesDevice(WInputDevice *device, const QList<QRegularExpression> &rules)
{
    if (!device)
        return false;
        
    QString deviceName = device->name();
    WInputDevice::Type deviceType = device->type();
    QString devicePath = device->devicePath();

    QString deviceInfo = QString("%1:%2")
                        .arg(static_cast<int>(deviceType))
                        .arg(deviceName);
    QString deviceInfoWithPath = QString("%1:%2|%3")
                                .arg(static_cast<int>(deviceType))
                                .arg(deviceName)
                                .arg(devicePath);

    for (const auto &rule : rules) {
        if (rule.match(deviceInfo).hasMatch())
            return true;
        if (rule.match(deviceInfoWithPath).hasMatch())
            return true;
    }
    return false;
}

bool SeatsManager::deviceMatchesSeat(WInputDevice *device, WSeat *seat) const
{
    if (!device || !seat)
        return false;
        
    QString seatName = seat->name();
    
    if (!m_deviceRules.contains(seatName)) {
        return false;
    }

    bool matches = matchesDevice(device, m_deviceRules[seatName]);
    return matches;
}

WSeat *SeatsManager::findSeatForDevice(WInputDevice *device) const
{
    if (!device)
        return nullptr;
        
    for (auto seat : std::as_const(m_seats)) {
        if (seat->deviceList().contains(device)) {
            return seat;
        }
    }

    for (auto it = m_seats.begin(); it != m_seats.end(); ++it) {
        WSeat *seat = it.value();
        if (seat == m_defaultSeat) {
            continue;
        }
        if (deviceMatchesSeat(device, seat)) {
            return seat;
        }
    }

    WSeat *fallbackSeat = this->fallbackSeat();
    if (fallbackSeat) {
        bool hasRules = m_deviceRules.contains(fallbackSeat->name()) &&
                       !m_deviceRules[fallbackSeat->name()].isEmpty();

        if (hasRules && deviceMatchesSeat(device, fallbackSeat)) {
            return fallbackSeat;
        } else if (!hasRules) {
            return fallbackSeat;
        }
    }
    
    return nullptr;
}

void SeatsManager::setupAllSeats(QQuickWindow *renderWindow,
                                 WOutputLayout *layout,
                                 WSeatEventFilter *eventFilter,
                                 WCursor *seat0Cursor)
{
    for (auto *seat : std::as_const(m_seats)) {
        if (!seat->nativeHandle()) {
            qCWarning(treelandSeat) << "Seat" << seat->name() << "has no native handle, skipping configuration";
            continue;
        }

        // Setup cursor
        if (!seat->cursor()) {
            if (seat->name() == "seat0" && seat0Cursor) {
                seat->setCursor(seat0Cursor);
            } else {
                WCursor *cursor = new WCursor(seat);
                cursor->setParent(seat);
                if (layout) {
                    cursor->setLayout(layout);
                }
                seat->setCursor(cursor);
            }
        }

        seat->setKeyboardFocusWindow(renderWindow);

        if (!seat->eventFilter()) {
            seat->setEventFilter(eventFilter);
        }
    }
}

void SeatsManager::connectBackendSignals(WBackend *backend)
{
    if (!backend) {
        qCWarning(treelandSeat) << "Cannot connect signals for null backend";
        return;
    }

    connect(backend, &WBackend::inputAdded, this, [this](WInputDevice *device) {
        if (device) {
            Q_EMIT deviceAdded(device);
        }
    });

    connect(backend, &WBackend::inputRemoved, this, [this](WInputDevice *device) {
        if (device) {
            clearDeviceCache(device);
            // Detach device from its seat
            for (auto *seat : std::as_const(m_seats)) {
                if (seat->deviceList().contains(device)) {
                    seat->detachInputDevice(device);
                    qCDebug(treelandSeat) << "Device" << device->name() << "removed from seat" << seat->name();
                    break;
                }
            }

            Q_EMIT deviceRemoved(device);
        }
    });

}

void SeatsManager::assignExistingDevices(WBackend *backend)
{
    if (!backend) {
        qCWarning(treelandSeat) << "Cannot assign devices from null backend";
        return;
    }

    for (auto device : backend->inputDeviceList()) {
        if (device) {
            Q_EMIT deviceAdded(device);
        }
    }
}

void SeatsManager::assignDevice(WInputDevice *device,
                                QQuickWindow *renderWindow,
                                WOutputLayout *layout,
                                WSeat *fallbackSeat)
{
    if (!device) {
        qCWarning(treelandSeat) << "Cannot assign null device";
        return;
    }

    QString deviceName = device->name();
    WInputDevice::Type deviceType = device->type();

    // Filter out mismatched device types
    if (deviceType == WInputDevice::Type::Pointer && deviceName.contains("Keyboard", Qt::CaseInsensitive)) {
        qCWarning(treelandSeat) << "Device type mismatch - ignoring:" << deviceName;
        return;
    }

    if (deviceType == WInputDevice::Type::Keyboard &&
        (deviceName.contains("Mouse", Qt::CaseInsensitive) || deviceName.contains("Touchpad", Qt::CaseInsensitive))) {
        qCWarning(treelandSeat) << "Device type mismatch - ignoring:" << deviceName;
        return;
    }

    // Filter out system buttons
    if (deviceType == WInputDevice::Type::Keyboard &&
        (deviceName.contains("Power Button") || deviceName.contains("Sleep Button") ||
         deviceName.contains("Lid Switch") || deviceName.contains("Video Bus"))) {
        return;
    }

    // Auto-assign device to appropriate seat
    WSeat *assignedSeat = autoAssignDevice(device);
    if (!assignedSeat && fallbackSeat && fallbackSeat->nativeHandle()) {
        fallbackSeat->attachInputDevice(device);
        assignedSeat = fallbackSeat;
        qCDebug(treelandSeat) << "Device" << deviceName << "assigned to fallback seat";
    }

    if (!assignedSeat) {
        qCWarning(treelandSeat) << "Failed to assign device:" << deviceName;
        return;
    }

    // Setup cursor for pointer devices
    if (deviceType == WInputDevice::Type::Pointer && !assignedSeat->cursor()) {
        WCursor *cursor = new WCursor(assignedSeat);
        cursor->setParent(assignedSeat);
        if (layout) {
            cursor->setLayout(layout);
        }
        assignedSeat->setCursor(cursor);
    }

    // Setup keyboard focus window for keyboard devices
    if (deviceType == WInputDevice::Type::Keyboard && !assignedSeat->keyboardFocusWindow()) {
        assignedSeat->setKeyboardFocusWindow(renderWindow);
    }

    m_deviceCache[device] = assignedSeat;
}

WSeat *SeatsManager::getSeatForDevice(WInputDevice *device) const
{
    if (!device) {
        return nullptr;
    }

    // Check cache first
    if (m_deviceCache.contains(device)) {
        return m_deviceCache[device];
    }

    // Find seat and update cache
    WSeat *seat = findSeatForDevice(device);
    m_deviceCache[device] = seat;

    return seat;
}

WSeat *SeatsManager::getSeatForEvent(QInputEvent *event) const
{
    if (!event) {
        qCWarning(treelandSeat) << "getSeatForEvent called with null event";
        return nullptr;
    }

    if (event->device()) {
        WInputDevice *device = WInputDevice::from(event->device());
        if (device) {
            WSeat *deviceSeat = device->seat();
            if (deviceSeat) {
                return deviceSeat;
            }
        }
    }

    return fallbackSeat();
}

void SeatsManager::clearDeviceCache(WInputDevice *device)
{
    m_deviceCache.remove(device);
}

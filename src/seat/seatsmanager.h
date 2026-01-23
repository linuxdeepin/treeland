// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>
#include <winputdevice.h>
#include <wserver.h>

#include <QObject>
#include <QMap>
#include <QRegularExpression>

QT_BEGIN_NAMESPACE
class QJsonObject;
class QInputEvent;
class QQuickWindow;
QT_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WBackend;
class WCursor;
class WOutputLayout;
class WSeatEventFilter;
WAYLIB_SERVER_END_NAMESPACE

// SeatsManager manages multi-seat configuration and device assignment for treeland.
// This class implements compositor-specific policies for seat creation, device
// assignment rules, and configuration persistence.
class SeatsManager : public QObject
{
    Q_OBJECT

public:
    explicit SeatsManager(WServer *server, QObject *parent = nullptr);
    ~SeatsManager();

    // Seat management
    WSeat *createSeat(const QString &name, bool isFallback = false);
    void removeSeat(const QString &name);
    void removeSeat(WSeat *seat);
    WSeat *getSeat(const QString &name) const;
    QList<WSeat*> seats() const;
    WSeat *fallbackSeat() const;

    // Device assignment
    void assignDeviceToSeat(WInputDevice *device, const QString &seatName);
    WSeat *autoAssignDevice(WInputDevice *device);

    // Device lifecycle management
    void assignDevice(WInputDevice *device,
                     QQuickWindow *renderWindow,
                     WAYLIB_SERVER_NAMESPACE::WOutputLayout *layout,
                     WSeat *fallbackSeat);
    void setupAllSeats(QQuickWindow *renderWindow,
                      WAYLIB_SERVER_NAMESPACE::WOutputLayout *layout,
                      WAYLIB_SERVER_NAMESPACE::WSeatEventFilter *eventFilter,
                      WAYLIB_SERVER_NAMESPACE::WCursor *seat0Cursor = nullptr);
    void connectBackendSignals(WAYLIB_SERVER_NAMESPACE::WBackend *backend);
    void assignExistingDevices(WAYLIB_SERVER_NAMESPACE::WBackend *backend);

    // Device query with cache
    WSeat *getSeatForDevice(WInputDevice *device) const;
    WSeat *getSeatForEvent(QInputEvent *event) const;
    void clearDeviceCache(WInputDevice *device);

    // TODO(Lyn): Replace file-based seat config (loadSeatConfig/saveSeatConfig) with DConfig.
    // Configuration management
    void loadConfig(const QJsonObject &config);
    QJsonObject saveConfig() const;

    // Configuration file operations
    bool loadConfigFromFile(const QString &filePath);
    bool saveConfigToFile(const QString &filePath) const;
    bool validateConfig(const QJsonObject &config) const;

    // Initialization
    void initializeDefaultSeat(WServer *server);
    void ensureDefaultDeviceRules(const QString &seatName);

    // Initialize seats from config file with fallback to default
    WSeat *initializeFromConfig(const QString &configPath, WServer *server);

    // Device matching rules
    void addDeviceRule(const QString &seatName, const QString &rule);
    void removeDeviceRule(const QString &seatName, const QString &rule);
    QStringList deviceRules(const QString &seatName) const;

    // Device matching (moved from WSeat)
    static bool matchesDevice(WInputDevice *device, const QList<QRegularExpression> &rules);
    bool deviceMatchesSeat(WInputDevice *device, WSeat *seat) const;
    WSeat *findSeatForDevice(WInputDevice *device) const;

Q_SIGNALS:
    void seatAdded(WSeat *seat);
    void seatRemoved(WSeat *seat);
    void deviceReassigned(WInputDevice *device, WSeat *oldSeat, WSeat *newSeat);
    void deviceAdded(WInputDevice *device);
    void deviceRemoved(WInputDevice *device);

private:
    WServer *m_server = nullptr;
    QMap<QString, WSeat*> m_seats;
    QMap<QString, QList<QRegularExpression>> m_deviceRules;
    WSeat *m_defaultSeat = nullptr;
    mutable QMap<WInputDevice*, WSeat*> m_deviceCache;
};

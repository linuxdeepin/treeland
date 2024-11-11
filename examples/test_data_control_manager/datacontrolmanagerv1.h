// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-wlr-data-control-unstable-v1.h"

#include <QMimeData>
#include <QObject>
#include <QWaylandClientExtension>

class Clipboard;

class DataControlDeviceV1ManagerV1
    : public QWaylandClientExtensionTemplate<DataControlDeviceV1ManagerV1>
    , public QtWayland::zwlr_data_control_manager_v1
{
    Q_OBJECT
public:
    DataControlDeviceV1ManagerV1();
    ~DataControlDeviceV1ManagerV1();

    void instantiate();
};

class DataControlOfferV1
    : public QMimeData
    , public QtWayland::zwlr_data_control_offer_v1
{
    Q_OBJECT
public:
    DataControlOfferV1(struct ::zwlr_data_control_offer_v1 *id);
    ~DataControlOfferV1();

    QStringList formats() const override;
    bool containsImageData() const;
    bool hasFormat(const QString &mimeType) const override;
    QVariant retrieveData(const QString &mimeType) const;

protected:
    void zwlr_data_control_offer_v1_offer(const QString &mime_type) override;

private:
    static bool readData(int fd, QByteArray &data);
    QStringList m_receivedFormats;

    friend class DataControlDeviceV1;
};

class DataControlSourceV1
    : public QObject
    , public QtWayland::zwlr_data_control_source_v1
{
    Q_OBJECT
public:
    DataControlSourceV1(struct ::zwlr_data_control_source_v1 *id, QMimeData *mimeData);
    DataControlSourceV1() = default;
    ~DataControlSourceV1();

    QMimeData *mimeData();
    std::unique_ptr<QMimeData> releaseMimeData();

Q_SIGNALS:
    void cancelled();

protected:
    void zwlr_data_control_source_v1_send(const QString &mime_type, int32_t fd) override;
    void zwlr_data_control_source_v1_cancelled() override;

private:
    std::unique_ptr<QMimeData> m_mimeData;
};

class DataControlDeviceV1
    : public QObject
    , public QtWayland::zwlr_data_control_device_v1
{
    Q_OBJECT
public:
    DataControlDeviceV1(struct ::zwlr_data_control_device_v1 *id);
    ~DataControlDeviceV1();

    void setSelection(std::unique_ptr<DataControlSourceV1> selection);
    QMimeData *receivedSelection();
    QMimeData *selection();

    void setPrimarySelection(std::unique_ptr<DataControlSourceV1> selection);
    QMimeData *receivedPrimarySelection();
    QMimeData *primarySelection();

Q_SIGNALS:
    void receivedSelectionChanged();
    void selectionChanged();

    void receivedPrimarySelectionChanged();
    void primarySelectionChanged();

protected:
    void zwlr_data_control_device_v1_data_offer(struct ::zwlr_data_control_offer_v1 *id) override;
    void zwlr_data_control_device_v1_selection(struct ::zwlr_data_control_offer_v1 *id) override;
    void zwlr_data_control_device_v1_primary_selection(
        struct ::zwlr_data_control_offer_v1 *id) override;

private:
    std::unique_ptr<DataControlSourceV1> m_selection;
    std::unique_ptr<DataControlOfferV1> m_receivedSelection;

    std::unique_ptr<DataControlSourceV1> m_primarySelection;
    std::unique_ptr<DataControlOfferV1> m_receivedPrimarySelection;

    friend class Clipboard;
};

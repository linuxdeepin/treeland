// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "clipboard.h"

#include <QBuffer>
#include <QGuiApplication>

Clipboard::Clipboard(QObject *parent)
    : QObject{parent}
{
    m_manager.reset(new DataControlDeviceV1ManagerV1);
    QObject::connect(m_manager.get(), &DataControlDeviceV1ManagerV1::activeChanged, this, [this]() {
        if (m_manager->isActive()) {
            auto waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
            if (!waylandApp) {
                return;
            }
            auto seat = waylandApp->seat();

            if (!seat)
                qFatal("Failed to get wl_seat frome QtWayland QPA!");

            m_device.reset(new DataControlDeviceV1(m_manager->get_data_device(seat)));

            connect(m_device.get(), &DataControlDeviceV1::receivedSelectionChanged, this, [this]() {
                if (!m_device->selection()) {
                    Q_EMIT changed(QClipboard::Clipboard);

                    if (m_device->m_receivedSelection) {
                        DataControlOfferV1 *offer = m_device->m_receivedSelection.get();
                        for(auto type : offer->formats()) {
                            QVariant data = offer->retrieveData(type);
                            qWarning() << "----------receivedSelectionChanged---" << "type:=" << type << "data:= " << data;
                        }
                    }
                }
            });
            connect(m_device.get(), &DataControlDeviceV1::selectionChanged, this, [this]() {
                Q_EMIT changed(QClipboard::Clipboard);
            });

            connect(m_device.get(), &DataControlDeviceV1::receivedPrimarySelectionChanged, this, [this]() {
                if (!m_device->primarySelection()) {
                    Q_EMIT changed(QClipboard::Selection);

                    if (m_device->m_receivedPrimarySelection) {
                        DataControlOfferV1 *offer = m_device->m_receivedPrimarySelection.get();
                        for(auto type : offer->formats()) {
                            QVariant data = offer->retrieveData(type);
                            qWarning() << "----------receivedPrimarySelectionChanged" << "type:=" << type << "data:= " << data;
                        }
                    }
                }
            });
            connect(m_device.get(), &DataControlDeviceV1::primarySelectionChanged, this, [this]() {
                Q_EMIT changed(QClipboard::Selection);
            });

        } else {
            m_device.reset();
        }
    });

    m_manager->instantiate();
}

bool Clipboard::isValid()
{
    return m_manager && m_manager->isInitialized();
}

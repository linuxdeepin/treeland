// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualoutputmanager.h"

#include <QDebug>

#define VIRTUAL_OUTPUT_MANAGER_V1_VERSION 1

VirtualOutputManager::VirtualOutputManager()
    : QWaylandClientExtensionTemplate<VirtualOutputManager>(
        VIRTUAL_OUTPUT_MANAGER_V1_VERSION)
{
}

VirtualOutputManager::~VirtualOutputManager()
{
    if (isInitialized()) {
        destroy();
    }
}

void VirtualOutputManager::instantiate()
{
    initialize();
}

struct ::treeland_virtual_output_v1 *
VirtualOutputManager::createVirtualOutput(const QString &name, const QByteArray &outputs)
{
    if (!isInitialized()) {
        return nullptr;
    }
    return create_virtual_output(name, outputs);
}

void VirtualOutputManager::getVirtualOutputList()
{
    if (!isInitialized()) {
        return;
    }
    get_virtual_output_list();
}

struct ::treeland_virtual_output_v1 *
VirtualOutputManager::getVirtualOutput(const QString &name)
{
    if (!isInitialized()) {
        return nullptr;
    }
    return get_virtual_output(name);
}

void VirtualOutputManager::treeland_virtual_output_manager_v1_virtual_output_list(
    wl_array *names)
{
    if (!names || names->size == 0)
        return;

    char *data = static_cast<char *>(names->data);
    char *end = data + names->size;
    QStringList nameList;

    while (data < end && *data != '\0') {
        QString name = QString::fromUtf8(data);
        nameList << name;
        data += name.size() + 1;
    }

    qInfo() << "Virtual output list:" << nameList;
    Q_EMIT virtualOutputListReceived(nameList);
}

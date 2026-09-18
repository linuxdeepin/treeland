// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualoutput.h"

#include <QDebug>

VirtualOutput::VirtualOutput(struct ::treeland_virtual_output_v1 *object)
    : QtWayland::treeland_virtual_output_v1(object)
{
}

VirtualOutput::~VirtualOutput()
{
}

void VirtualOutput::treeland_virtual_output_v1_outputs(const QString &name, wl_array *outputs)
{
    QStringList outputList;

    if (outputs && outputs->size > 0) {
        char *data = static_cast<char *>(outputs->data);
        char *end = data + outputs->size;

        while (data < end && *data != '\0') {
            QString output = QString::fromUtf8(data);
            outputList << output;
            data += output.size() + 1;
        }
    }

    if (outputList.isEmpty()) {
        qInfo() << "Screen group name:" << name << "(not found or empty)";
    } else {
        qInfo() << "Screen group name:" << name;
        qInfo() << "  Outputs:" << outputList;
    }

    // Always emit, even for an empty list, so the client observes the
    // required empty outputs event for a vanished group.
    Q_EMIT outputsReceived(name, outputList);
}

void VirtualOutput::treeland_virtual_output_v1_error(uint32_t code, const QString &message)
{
    qInfo() << "error code:" << code << " error message:" << message;
    Q_EMIT errorOccurred(code, message);
}

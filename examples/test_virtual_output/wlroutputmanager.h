// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-wlr-output-management-unstable-v1.h"

#include <QPoint>
#include <QSize>
#include <QVector>
#include <QWaylandClientExtension>

#include <memory>

class WlrOutputManagerPrivate;

class WlrOutputManager
    : public QWaylandClientExtensionTemplate<WlrOutputManager>
    , public QtWayland::zwlr_output_manager_v1
{
    Q_OBJECT
public:
    struct Mode {
        QSize size;
        int refresh = 0; // mHz, 0 means unspecified
        bool preferred = false;
    };

    struct Head {
        QString name;
        QString description;
        bool enabled = false;
        QPoint position;
        int transform = 0; // wl_output_transform enum
        double scale = 1.0;
        QSize physicalSize;
        QVector<Mode> modes;
        int currentMode = -1; // index into modes, -1 if unknown
    };

    explicit WlrOutputManager();
    ~WlrOutputManager() override;

    void instantiate();

    QVector<Head> heads() const;
    uint32_t serial() const;

    void applyConfig(const QString &headName,
                     bool enabled,
                     const QPoint &position,
                     const QSize &modeSize,
                     int refresh,
                     int transform,
                     double scale);

Q_SIGNALS:
    void headsChanged();
    void applyFinished(bool ok);

protected:
    void zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head) override;
    void zwlr_output_manager_v1_done(uint32_t serial) override;
    void zwlr_output_manager_v1_finished() override;

private:
    std::unique_ptr<WlrOutputManagerPrivate> d;
};
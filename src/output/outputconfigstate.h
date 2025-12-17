// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QString>
#include <QMap>
#include <QObject>

struct OutputPrimaryState {
    bool wasPrimary = false;
};

class OutputConfigState : public QObject {
    Q_OBJECT
public:
    explicit OutputConfigState(QObject *parent = nullptr) : QObject(parent) {}
    ~OutputConfigState() = default;

    void markScreenAsPrimary(const QString &outputName);
    bool wasScreenPrimary(const QString &outputName) const;
    void clearOutputState(const QString &outputName);
    void recordCopyModeExit();
    bool shouldRestoreCopyMode() const;
    void clearCopyModeIntent();

private:
    QMap<QString, OutputPrimaryState> m_states;
    bool m_copyModeExited = false;  // Flag indicating Copy Mode was exited and should be restored
};


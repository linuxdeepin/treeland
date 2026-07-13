// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <woutputmanagerv1.h>

#include <functional>

class Output;
class RootSurfaceContainer;
class SurfaceWrapper;
class TreelandConfig;

class OutputManager : public QObject {
    Q_OBJECT
public:
    enum class Mode
    {
        Extension,
        Copy
    };

    struct CopyModeRestoreConfig
    {
        Output *primaryOutput = nullptr;
        QString name;
        QStringList outputIds;
        QStringList outputNames;

        explicit operator bool() const { return primaryOutput && outputIds.size() >= 2; }
    };

    explicit OutputManager(RootSurfaceContainer *rootContainer,
                           TreelandConfig *config,
                           QObject *parent = nullptr);
    ~OutputManager() override = default;

    void setMode(Mode mode);
    Mode mode() const;

    void onScreenAdded(Output *output, const QList<SurfaceWrapper *> &surfaces);
    bool onScreenRemoved(Output *output, const QList<SurfaceWrapper *> &surfaces);
    void onScreenDisabled(Output *output, const QList<SurfaceWrapper *> &surfaces);
    void onScreenEnabled(Output *output);

    bool shouldRestoreCopyMode(int availableOutputCount) const;
    bool restoreConfiguredSingleOutput(const QList<SurfaceWrapper *> &surfaces, bool updatePrimaryOutputConfig = false);
    void restorePrimaryOutput();
    CopyModeRestoreConfig copyModeRestoreConfig(int availableOutputCount) const;
    QStringList copyOutputIds() const;
    QStringList currentOutputIds(Output *primaryOutput = nullptr) const;
    QStringList outputNamesFromIds(const QStringList &ids) const;
    void storeSingleOutputConfig();
    void clearSingleOutputConfig();
    void storeCopyOutputConfig(bool enabled,
                               const QString &name = {},
                               const QStringList &outputIds = {});
    bool takeCopyModeRestoreIntent();
    void setCopyModeStored(bool enabled);
    void clearCopyModeRestoreIntent();

Q_SIGNALS:
    void copyOutputConfigurationChanged(bool enabled,
                                        const QString &name,
                                        const QStringList &outputNames);

private:
    Output *findFirstAvailableOutput(Output *excludeOutput) const;
    void markScreenAsPrimaryIntent(Output *output);
    void restoreScreenAsPrimary(Output *output);
    void switchPrimaryOutput(Output *from, Output *to, const QList<SurfaceWrapper *> &surfaces);
    QString outputId(Output *output) const;
    Output *findOutputById(const QString &id) const;
    void runWhenConfigInitialized(std::function<void()> callback);

    RootSurfaceContainer *m_rootContainer = nullptr;
    TreelandConfig *m_config = nullptr;
    Mode m_mode = Mode::Extension;
    bool m_copyModeRestoreIntent = false;
    QMap<QString, bool> m_primaryRestoreIntents;
};

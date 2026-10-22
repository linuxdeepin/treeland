// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include "modules/output-group/outputgroupmanagerinterfacev1.h"

#include <woutputmanagerv1.h>
#include <wglobal.h>

class Output;
class OutputConfig;
class OutputGroupManagerInterfaceV1;
class RootSurfaceContainer;
class SessionManager;
class SurfaceWrapper;
class TreelandConfig;
class WallpaperManager;
class Workspace;
struct wlr_output;
struct wlr_gamma_control_manager_v1;
struct wlr_gamma_control_manager_v1_set_gamma_event;
struct wlr_output_power_manager_v1;
struct wlr_output_power_v1_set_mode_event;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
class WBackend;
class WListenerOwner;
class WOutputRenderWindow;
class WOutputViewport;
class WXdgOutputManager;
WAYLIB_SERVER_END_NAMESPACE

class OutputManager
    : public QObject
    , public OutputGroupHost
{
    Q_OBJECT
public:
    enum class Mode
    {
        Extension,
        Copy
    };

    explicit OutputManager(RootSurfaceContainer *rootContainer,
                           TreelandConfig *config,
                           WallpaperManager *wallpaperManager,
                           Workspace *workspace,
                           WAYLIB_SERVER_NAMESPACE::WOutputRenderWindow *renderWindow,
                           QObject *parent = nullptr);
    ~OutputManager() override = default;

    Mode mode() const;
    void setBackend(WAYLIB_SERVER_NAMESPACE::WBackend *backend);
    void setOutputManagementProtocol(WAYLIB_SERVER_NAMESPACE::WOutputManagerV1 *manager);
    void setOutputGroupInterface(OutputGroupManagerInterfaceV1 *interface);
    void setGammaControlManager(wlr_gamma_control_manager_v1 *manager);
    void setOutputPowerManager(wlr_output_power_manager_v1 *manager);
    void setXdgOutputManagers(WAYLIB_SERVER_NAMESPACE::WXdgOutputManager *manager,
                              WAYLIB_SERVER_NAMESPACE::WXdgOutputManager *xwaylandManager,
                              SessionManager *sessionManager);
    WAYLIB_SERVER_NAMESPACE::WXdgOutputManager *xwaylandOutputManager() const;

    QList<SurfaceWrapper *> workspaceSurfaces(Output *filterOutput = nullptr) const;
    Output *outputFor(WAYLIB_SERVER_NAMESPACE::WOutput *output) const;
    Output *outputAtCursor() const;
    const QList<Output *> &outputs() const;
    void requestAdditionalOutputs();
    void applyMode(Mode mode);
    void wakePoweredOffOutputs();

    void backendStarted();

    // OutputGroupHost
    bool isOutputAvailable(const QString &uuid) const override;
    bool canReconfigureOutputs() const override;
    void applyOutputGroups() override;

Q_SIGNALS:
    void modeChanged();

private Q_SLOTS:
    void continueOutputAdded();

private:
    void setGamma(wlr_gamma_control_manager_v1_set_gamma_event *event);
    void setPowerMode(wlr_output_power_v1_set_mode_event *event);
    void testOrApplyConfiguration(wlr_output_configuration_v1 *config, bool onlyTest);
    void outputAdded(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void outputRemoved(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void processOutputAdded(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    bool isNvidiaCardPresent() const;

    void onScreenAdded(Output *output, const QList<SurfaceWrapper *> &surfaces);
    bool onScreenRemoved(Output *output, const QList<SurfaceWrapper *> &surfaces);
    void onScreenDisabled(Output *output, const QList<SurfaceWrapper *> &surfaces);
    void onScreenEnabled(Output *output);

    bool shouldRestoreCopyMode(int availableOutputCount) const;
    bool restoreConfiguredSingleOutput(const QList<SurfaceWrapper *> &surfaces, bool updatePrimaryOutputConfig = false);
    void restorePrimaryOutput();
    void storeSingleOutputConfig();
    void clearSingleOutputConfig();
    // Converts every mirror output back to a normal output; used before the
    // output-management protocol reconfigures the outputs itself.
    void deactivateGroupMirrors();
    Output *convertMirrorToNormal(int index);
    void notifyOutputGroupsChanged();

    Output *createNormalOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    Output *createCopyOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output, Output *proxy);
    bool ensureInRootContainer(Output *output);
    void removeFromRootContainer(Output *output);
    void removeFromRootContainer(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    WAYLIB_SERVER_NAMESPACE::WOutputViewport *ownOutputViewport(
        WAYLIB_SERVER_NAMESPACE::WOutput *output) const;
    void moveSurfaces(const QList<SurfaceWrapper *> &surfaces,
                      Output *targetOutput,
                      Output *sourceOutput);
    void saveCurrentConfig(Output *output);
    int indexOf(WAYLIB_SERVER_NAMESPACE::WOutput *output) const;
    Output *findOutputByName(const QString &name) const;
    Output *findOutputById(const QString &id) const;
    void allowNonDrmOutputAutoChangeMode(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void enableAllOutputs();
    void restoreExtensionModeFromConfig(bool preserveSingleOutputConfig = false);
    void restoreInitialOutputConfiguration();
    void finishInitialOutputScanIfReady();
    Output *findFirstAvailableOutput(Output *excludeOutput) const;
    Output *findOutputByUuid(const QString &uuid) const;
    void markScreenAsPrimaryIntent(Output *output);
    void restoreScreenAsPrimary(Output *output);
    void switchPrimaryOutput(Output *from, Output *to, const QList<SurfaceWrapper *> &surfaces);
    RootSurfaceContainer *m_rootContainer = nullptr;
    TreelandConfig *m_config = nullptr;
    WallpaperManager *m_wallpaperManager = nullptr;
    Workspace *m_workspace = nullptr;
    WAYLIB_SERVER_NAMESPACE::WOutputRenderWindow *m_renderWindow = nullptr;
    WAYLIB_SERVER_NAMESPACE::WBackend *m_backend = nullptr;
    WAYLIB_SERVER_NAMESPACE::WOutputManagerV1 *m_outputManagementProtocol = nullptr;
    WAYLIB_SERVER_NAMESPACE::WXdgOutputManager *m_xwaylandOutputManager = nullptr;
    OutputGroupManagerInterfaceV1 *m_outputGroupManager = nullptr;
    WAYLIB_SERVER_NAMESPACE::WListenerOwner m_gammaControlListenerOwner;
    WAYLIB_SERVER_NAMESPACE::WListenerOwner m_outputPowerListenerOwner;
    WAYLIB_SERVER_NAMESPACE::WListenerOwner m_outputStateListenerOwner;
    Mode m_mode = Mode::Extension;
    // Name of the group created by the menu bar copy-mode toggle, so the
    // toggle only dissolves its own group.
    QString m_menuBarGroupName;
    bool m_applyingOutputGroups = false;
    // A group re-apply was requested while an output-management apply was in
    // flight; it runs once the last commit job finished.
    bool m_groupsDirty = false;
    int m_inFlightOutputCommits = 0;
    QMap<QString, bool> m_primaryRestoreIntents;
    QList<Output *> m_outputs;
    QMap<WAYLIB_SERVER_NAMESPACE::WOutput *, OutputConfig *> m_pendingOutputs;
    QSet<wlr_output *> m_powerOffOutputs;
    bool m_backendStartFinished = false;
    bool m_initialOutputScanFinished = false;

    struct PendingOutputConfig
    {
        wlr_output_configuration_v1 *config = nullptr;
        QList<WAYLIB_SERVER_NAMESPACE::WOutputState> states;
        int pendingCommits = 0;
        bool allSuccess = true;
    };
    PendingOutputConfig m_pendingOutputConfig;

    void onOutputCommitFinished(wlr_output_configuration_v1 *config, bool success);
};

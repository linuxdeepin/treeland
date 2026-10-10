// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <woutputmanagerv1.h>
#include <wglobal.h>

class Output;
class RootSurfaceContainer;
class SessionManager;
class SurfaceWrapper;
class TreelandConfig;
class VirtualOutputInterfaceV1;
class VirtualOutputManagerInterfaceV1;
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

class OutputManager : public QObject {
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
    void setVirtualOutputInterface(VirtualOutputManagerInterfaceV1 *interface);
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

Q_SIGNALS:
    void modeChanged();

private:
    struct CopyModeRestoreConfig
    {
        Output *primaryOutput = nullptr;
        QString name;
        QStringList outputIds;
        QStringList outputNames;

        explicit operator bool() const { return primaryOutput && outputIds.size() >= 2; }
    };

    void createVirtualOutput(VirtualOutputInterfaceV1 *interface);
    void destroyVirtualOutput(VirtualOutputInterfaceV1 *interface);
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
    CopyModeRestoreConfig copyModeRestoreConfig(int availableOutputCount) const;
    QStringList copyOutputIds() const;
    QStringList currentOutputIds(Output *primaryOutput = nullptr) const;
    QStringList outputNamesFromIds(const QStringList &ids) const;
    void storeSingleOutputConfig();
    void clearSingleOutputConfig();
    void storeCopyOutputConfig(bool enabled,
                               const QString &name = {},
                               const QStringList &outputIds = {});
    void setCopyModeStored(bool enabled);
    void clearCopyModeRestoreIntent();

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
    void applyCopyModeToOutputs(Output *primaryOutput,
                                const QList<SurfaceWrapper *> &surfaces,
                                const QStringList &outputIds = {},
                                bool persistConfig = true);
    bool restoreConfiguredCopyMode();
    void restoreExtensionModeFromConfig(bool preserveSingleOutputConfig = false);
    void restoreInitialOutputConfiguration();
    void handleCopyModeOutputDisable(Output *affectedOutput);
    void finishInitialOutputScanIfReady();
    Output *findFirstAvailableOutput(Output *excludeOutput) const;
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
    VirtualOutputManagerInterfaceV1 *m_virtualOutputInterface = nullptr;
    WAYLIB_SERVER_NAMESPACE::WListenerOwner m_outputListenerOwner;
    Mode m_mode = Mode::Extension;
    bool m_copyModeRestoreIntent = false;
    QMap<QString, bool> m_primaryRestoreIntents;
    QList<Output *> m_outputs;
    QSet<WAYLIB_SERVER_NAMESPACE::WOutput *> m_pendingOutputs;
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

// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include "modules/capture/capture.h"
#include "utils/cmdline.h"
#include "utils/fpsdisplaymanager.h"
#include "modules/dde-shell/ddeshellattached.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "input/inputdevice.h"
#include "core/layersurfacecontainer.h"
#include "greeter/usermodel.h"
#ifdef EXT_SESSION_LOCK_V1
#include "wsessionlock.h"
#include "wsessionlockmanager.h"
#endif

#include <rhi/qrhi.h>

#if !defined(DISABLE_DDM) || defined(EXT_SESSION_LOCK_V1)
#  include "core/lockscreen.h"
#endif
#include "interfaces/multitaskviewinterface.h"
#include "output/output.h"
#include "modules/output-manager/outputmanagement.h"
#include "modules/personalization/personalizationmanager.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "modules/shortcut/shortcutmanager.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "input/togglablegesture.h"
#include "modules/wallpaper-color/wallpapercolor.h"
#include "core/windowpicker.h"
#include "workspace/workspace.h"
#include "common/treelandlogging.h"
#include "modules/ddm/ddminterfacev1.h"
#include "treelandconfig.hpp"
#include "core/treeland.h"
#include "greeter/greeterproxy.h"
#include "modules/screensaver/screensaverinterfacev1.h"
#include "xsettings/settingmanager.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <WBackend>
#include <WForeignToplevel>
#include <WOutput>
#include <WServer>
#include <WSurfaceItem>
#include <WXdgOutput>
#include <wcursorshapemanagerv1.h>
#include <wlayersurface.h>
#include <woutputhelper.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <woutputmanagerv1.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wqmlcreator.h>
#include <wquickcursor.h>
#include <wrenderhelper.h>
#include <wseat.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgshell.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wextimagecapturesourcev1impl.h>
#include <wsecuritycontextmanager.h>

#include <qwallocator.h>
#include <qwbackend.h>
#include <qwbuffer.h>
#include <qwcompositor.h>
#include <qwdatacontrolv1.h>
#include <qwdatadevice.h>
#include <qwdisplay.h>
#include <qwextdatacontrolv1.h>
#include <qwextimagecopycapturev1.h>
#include <qwextimagecapturesourcev1.h>
#include <qwextforeigntoplevelimagecapturesourcemanagerv1.h>
#include <qwextforeigntoplevellistv1.h>
#include <qwfractionalscalemanagerv1.h>
#include <qwgammacontorlv1.h>
#include <qwlayershellv1.h>
#include <qwlogging.h>
#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwscreencopyv1.h>
#include <qwsession.h>
#include <qwsubcompositor.h>
#include <qwviewporter.h>
#include <qwxwaylandsurface.h>
#include <qwoutputpowermanagementv1.h>
#include <qwidlenotifyv1.h>
#include <qwidleinhibitv1.h>
#include <qwalphamodifierv1.h>
#include <qwdrm.h>
#include <qwxwayland.h>

#include <QAction>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtConcurrent>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>

#include <pwd.h>
#include <utility>
#include <functional>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <wayland-util.h>

#define WLR_FRACTIONAL_SCALE_V1_VERSION 1
#define EXT_DATA_CONTROL_MANAGER_V1_VERSION 1
#define _DEEPIN_NO_TITLEBAR "_DEEPIN_NO_TITLEBAR"

static xcb_atom_t internAtom(xcb_connection_t *connection, const char *name, bool onlyIfExists)
{
    if (!name || *name == 0)
        return XCB_NONE;

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, onlyIfExists, strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, 0);

    if (!reply)
        return XCB_NONE;

    xcb_atom_t atom = reply->atom;
    free(reply);

    return atom;
}

static QByteArray readWindowProperty(xcb_connection_t *connection,
                                     xcb_window_t win,
                                     xcb_atom_t atom,
                                     xcb_atom_t type)
{
    QByteArray data;
    int offset = 0;
    int remaining = 0;

    do {
        xcb_get_property_cookie_t cookie =
            xcb_get_property(connection, false, win, atom, type, offset, 1024);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, NULL);
        if (!reply)
            break;

        remaining = 0;

        if (reply->type == type) {
            int len = xcb_get_property_value_length(reply);
            char *datas = (char *)xcb_get_property_value(reply);
            data.append(datas, len);
            remaining = reply->bytes_after;
            offset += len;
        }

        free(reply);
    } while (remaining > 0);

    return data;
}

Session::~Session()
{
    qCDebug(treelandCore) << "Deleting session for uid:" << uid << socket;
    Q_EMIT aboutToBeDestroyed();

    if (settingManagerThread) {
        settingManagerThread->quit();
        settingManagerThread->wait(QDeadlineTimer(25000));
    }

    if (settingManager) {
        delete settingManager;
        settingManager = nullptr;
    }
    if (xwayland)
        Helper::instance()->shellHandler()->removeXWayland(xwayland);
    if (socket)
        delete socket;
}

Helper *Helper::m_instance = nullptr;

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
    , m_renderWindow(new WOutputRenderWindow(this))
    , m_server(new WServer(this))
    , m_rootSurfaceContainer(new RootSurfaceContainer(m_renderWindow->contentItem()))
    , m_multiTaskViewGesture(new TogglableGesture(this))
    , m_windowGesture(new TogglableGesture(this))
{
    Q_ASSERT(!m_instance);
    m_instance = this;

    Q_ASSERT(!m_config);
    m_config = TreelandConfig::createByName("org.deepin.treeland",
                                            "org.deepin.treeland",
                                            QString());

    m_renderWindow->setColor(Qt::black);
    m_rootSurfaceContainer->setFlag(QQuickItem::ItemIsFocusScope, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    m_rootSurfaceContainer->setFocusPolicy(Qt::StrongFocus);
#endif

    m_shellHandler = new ShellHandler(m_rootSurfaceContainer);

#ifdef EXT_SESSION_LOCK_V1
    m_lockScreenGraceTimer = new QTimer(this);
    m_lockScreenGraceTimer->setInterval(300);
    m_lockScreenGraceTimer->setSingleShot(true);
#endif

    m_workspaceScaleAnimation = new QPropertyAnimation(m_shellHandler->workspace(), "scale", this);
    m_workspaceOpacityAnimation =
        new QPropertyAnimation(m_shellHandler->workspace(), "opacity", this);

    m_workspaceScaleAnimation->setDuration(1000);
    m_workspaceOpacityAnimation->setDuration(1000);
    m_workspaceScaleAnimation->setEasingCurve(QEasingCurve::OutExpo);
    m_workspaceOpacityAnimation->setEasingCurve(QEasingCurve::OutExpo);

    connect(m_renderWindow, &QQuickWindow::activeFocusItemChanged, this, [this]() {
        auto wrapper = keyboardFocusSurface();
        m_seat->setKeyboardFocusSurface(wrapper ? wrapper->surface() : nullptr);
    });

    connect(m_multiTaskViewGesture,
            &TogglableGesture::statusChanged,
            this,
            [this](TogglableGesture::Status status) {
                if (status == TogglableGesture::Inactive || status == TogglableGesture::Stopped) {
                    m_multitaskView->setStatus(IMultitaskView::Exited);
                } else {
                    m_multitaskView->setStatus(IMultitaskView::Active);
                }
                m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::Gesture);
            });

    connect(m_windowGesture, &TogglableGesture::activated, this, [this]() {
        auto surface = Helper::instance()->activatedSurface();
        if (m_currentMode == CurrentMode::Normal && surface && !surface->isMaximized()) {
            surface->requestMaximize();
        }
    });

    connect(m_windowGesture, &TogglableGesture::deactivated, this, [this]() {
        auto surface = Helper::instance()->activatedSurface();
        if (m_currentMode == CurrentMode::Normal && surface && surface->isMaximized()) {
            surface->requestCancelMaximize();
        }
    });

    connect(m_windowGesture, &TogglableGesture::longPressed, this, [this]() {
        auto surface = Helper::instance()->activatedSurface();
        if (m_currentMode == CurrentMode::Normal && surface && surface->isNormal()) {
            surface->requestMove();
        }
    });

    // Connect to systemd-logind's PrepareForSleep signal for hibernate blackout
    bool connected = QDBusConnection::systemBus().connect(
        "org.freedesktop.login1",           // service
        "/org/freedesktop/login1",          // path
        "org.freedesktop.login1.Manager",   // interface
        "PrepareForSleep",                  // signal name
        this,                               // receiver
        SLOT(onPrepareForSleep(bool))       // slot
    );

    if (!connected) {
        qCWarning(treelandCore) << "Failed to connect to systemd-logind PrepareForSleep signal";
    } else {
        qCInfo(treelandCore) << "Successfully connected to systemd-logind PrepareForSleep signal";
    }

    // Also connect to SessionNew signal for logging purposes
    QDBusConnection::systemBus().connect(
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        "SessionNew",
        this,
        SLOT(onSessionNew(QString,QDBusObjectPath))
    );
}

Helper::~Helper()
{
    for (auto session : std::as_const(m_sessions)) {
        Q_ASSERT(session);
        if (session->xwayland) {
            delete session->xwayland;
            session->xwayland = nullptr;
        }
        if (session->socket) {
            delete session->socket;
            session->socket = nullptr;
        }
    }
    m_sessions.clear();

    for (auto s : m_rootSurfaceContainer->surfaces()) {
        m_rootSurfaceContainer->destroyForSurface(s);
    }

    // destroy before m_rootSurfaceContainer
    delete m_shellHandler;
    delete m_rootSurfaceContainer;
    Q_ASSERT(m_instance == this);
    m_instance = nullptr;
}

Helper *Helper::instance()
{
    return m_instance;
}

TreelandConfig *Helper::config()
{
    return m_config;
}

bool Helper::isNvidiaCardPresent()
{
    auto rhi = m_renderWindow->rhi();

    if (!rhi)
        return false;

    QString deviceName = rhi->driverInfo().deviceName;
    qCDebug(treelandCore) << "Graphics Device:" << deviceName;

    return deviceName.contains("NVIDIA", Qt::CaseInsensitive);
}

void Helper::setWorkspaceVisible(bool visible)
{
    for (auto *surface : m_rootSurfaceContainer->surfaces()) {
        if (surface->type() == SurfaceWrapper::Type::Layer) {
            surface->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
        }
    }

    if (m_noAnimation) {
        m_shellHandler->workspace()->setOpacity(visible ? 1.0 : 0.0);
        m_shellHandler->workspace()->setScale(visible ? 1.0 : 1.4);
        return;
    }

    if (visible) {
        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.0);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(1.0);
        m_workspaceOpacityAnimation->start();
    } else {
        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.4);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(0.0);
        m_workspaceOpacityAnimation->start();
    }
}

QmlEngine *Helper::qmlEngine() const
{
    return qobject_cast<QmlEngine *>(::qmlEngine(this));
}

WOutputRenderWindow *Helper::window() const
{
    return m_renderWindow;
}

ShellHandler *Helper::shellHandler() const
{
    return m_shellHandler;
}

Workspace *Helper::workspace() const
{
    return m_shellHandler->workspace();
}

void Helper::onOutputAdded(WOutput *output)
{
    // TODO: 应该让helper发出Output的信号，每个需要output的单元单独connect。
    allowNonDrmOutputAutoChangeMode(output);
    Output *o = nullptr;
    if (m_mode == OutputMode::Extension || !m_rootSurfaceContainer->primaryOutput()) {
        o = createNormalOutput(output);
    } else if (m_mode == OutputMode::Copy) {
        o = createCopyOutput(output, m_rootSurfaceContainer->primaryOutput());
    }
    m_outputList.append(o);
    o->enable();
    m_outputManager->newOutput(output);

    m_wallpaperColorV1->updateWallpaperColor(output->name(),
                                             m_personalization->backgroundIsDark(output->name()));

    QString cache_location = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QSettings settings(cache_location + "/output.ini", QSettings::IniFormat);
    settings.beginGroup(QString("output.%1").arg(output->name()));
    if (settings.contains("scale") && m_mode != OutputMode::Copy) {
        qw_output_state newState;
        newState.set_enabled(true);

        int width = settings.value("width").toInt();
        int height = settings.value("height").toInt();
        int refresh = settings.value("refresh").toInt();

        wlr_output_mode *mode, *configMode = nullptr;
        wl_list_for_each(mode, &output->nativeHandle()->modes, link) {
            if (mode->width == width && mode->height == height && mode->refresh == refresh) {
                configMode = mode;
                break;
            }
        }
        if (configMode)
            newState.set_mode(configMode);
        else
            newState.set_custom_mode(width,
                                     height,
                                     refresh);

        newState.set_adaptive_sync_enabled(settings.value("adaptiveSyncEnabled").toBool());
        newState.set_transform(static_cast<wl_output_transform>(settings.value("transform").toInt()));
        newState.set_scale(settings.value("scale").toFloat());
        output->handle()->commit_state(newState);
    }
    settings.endGroup();
}

void Helper::onOutputRemoved(WOutput *output)
{
    auto index = indexOfOutput(output);
    Q_ASSERT(index >= 0);
    const auto o = m_outputList.takeAt(index);

    const auto &surfaces = getWorkspaceSurfaces(o);
    if (m_mode == OutputMode::Extension) {
        m_rootSurfaceContainer->removeOutput(o);
    } else if (m_mode == OutputMode::Copy) {
        m_mode = OutputMode::Extension;
        if (output == m_rootSurfaceContainer->primaryOutput()->output())
            m_rootSurfaceContainer->removeOutput(o);

        for (int i = 0; i < m_outputList.size(); i++) {
            if (m_outputList.at(i) == m_rootSurfaceContainer->primaryOutput())
                continue;
            Output *o1 = createNormalOutput(m_outputList.at(i)->output());
            o1->enable();
            m_outputList.at(i)->deleteLater();
            m_outputList.replace(i, o1);
        }
    }

    // When removing the last screen, no need to move the window position
    if (m_rootSurfaceContainer->primaryOutput() != o) {
        moveSurfacesToOutput(surfaces, m_rootSurfaceContainer->primaryOutput(), o);
    }

    m_outputManager->removeOutput(output);
    delete o;
}

void Helper::onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode)
{
    auto s = m_rootSurfaceContainer->getSurface(surface);
    if (!s)
        return;
    s->setNoDecoration(mode != WXdgDecorationManager::Server);
}

void Helper::setGamma(struct wlr_gamma_control_manager_v1_set_gamma_event *event)
{
    auto *qwOutput = qw_output::from(event->output);
    size_t ramp_size = 0;
    uint16_t *r = nullptr, *g = nullptr, *b = nullptr;
    wlr_gamma_control_v1 *gamma_control = event->control;
    if (gamma_control) {
        ramp_size = gamma_control->ramp_size;
        r = gamma_control->table;
        g = gamma_control->table + gamma_control->ramp_size;
        b = gamma_control->table + 2 * gamma_control->ramp_size;
    }
    qw_output_state newState;
    newState.set_gamma_lut(ramp_size, r, g, b);
    if (!qwOutput->commit_state(newState)) {
        qCWarning(treelandCore) << "Failed to set gamma lut!";
        // TODO: use software impl it.
        qw_gamma_control_v1::from(gamma_control)->send_failed_and_destroy();
    }
}

void Helper::handleCopyModeOutputDisable(Output *affectedOutput)
{
    int affectedIndex = m_outputList.indexOf(affectedOutput);
    if (affectedIndex < 0) {
        qCWarning(treelandCore) << "Disabled output not found in m_outputList";
        return;
    }

    m_mode = OutputMode::Extension;
    Q_EMIT outputModeChanged();

    // Convert CopyOutputs to Normal outputs (independent displays)
    // Keep the disabled output in the list - it will receive disable state through normal wlroots flow
    Output *primaryCandidate = nullptr;
    const auto &surfaces = getWorkspaceSurfaces(affectedOutput);
    for (int i = 0; i < m_outputList.size(); i++) {
        if (i == affectedIndex) {
            continue;
        }

        Output *copyOutput = m_outputList.at(i);
        Output *normalOutput = createNormalOutput(copyOutput->output());
        normalOutput->enable();
        copyOutput->deleteLater();
        m_outputList.replace(i, normalOutput);

        if (!primaryCandidate) {
            primaryCandidate = normalOutput;
        }
    }

    if (primaryCandidate) {
        if (!surfaces.isEmpty()) {
            moveSurfacesToOutput(surfaces, primaryCandidate, affectedOutput);
        }
        m_rootSurfaceContainer->setPrimaryOutput(primaryCandidate);
    }
}


void Helper::onOutputTestOrApply(qw_output_configuration_v1 *config, bool onlyTest)
{
    QList<WOutputState> states = m_outputManager->stateListPending();

    if (onlyTest) {
        bool ok = true;
        for (const auto &state : std::as_const(states)) {
            WOutputViewport *viewport = getOwnOutputViewport(state.output);
            if (!viewport) {
                ok = false;
                continue;
            }

            WOutputRenderWindow *renderWindow = viewport->outputRenderWindow();
            if (!renderWindow) {
                ok = false;
                continue;
            }
            qw_output_state newState;
            newState.set_enabled(state.enabled);
            if (state.enabled) {
                if (state.mode)
                    newState.set_mode(state.mode);
                else
                    newState.set_custom_mode(state.customModeSize.width(),
                                             state.customModeSize.height(),
                                             state.customModeRefresh);
                newState.set_adaptive_sync_enabled(state.adaptiveSyncEnabled);
                newState.set_transform(static_cast<wl_output_transform>(state.transform));
                newState.set_scale(state.scale);
            }
            ok &= state.output->handle()->test_state(newState);
        }

        m_outputManager->sendResult(config, ok);
        return;
    }

    if (m_pendingOutputConfig.config) {
        m_outputManager->sendResult(m_pendingOutputConfig.config, false);
    }

    // Handle Copy Mode transition when primary output is disabled
    if (m_mode == OutputMode::Copy) {
        for (const auto &state : std::as_const(states)) {
            if (!state.enabled) {
                Output *affectedOutput = getOutput(state.output);
                if (affectedOutput && affectedOutput == m_rootSurfaceContainer->primaryOutput()) {
                    handleCopyModeOutputDisable(affectedOutput);
                    break;
                }
            }
        }
    }

    m_pendingOutputConfig.config = config;
    m_pendingOutputConfig.states = states;
    m_pendingOutputConfig.pendingCommits = 0;
    m_pendingOutputConfig.allSuccess = true;

    for (const auto &state : std::as_const(states)) {
        // Skip outputs that have been removed (e.g., disabled in Copy mode)
        Output *output = getOutput(state.output);
        if (!output) {
            continue;
        }

        WOutputViewport *viewport = getOwnOutputViewport(state.output);
        if (!viewport) {
            m_outputManager->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        WOutputRenderWindow *renderWindow = viewport->outputRenderWindow();
        if (!renderWindow) {
            qCWarning(treelandCore) << "No renderWindow for output" << state.output->name();
            m_outputManager->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        if (state.enabled) {
            auto outputItem = qobject_cast<WOutputItem*>(viewport->parentItem());
            if (outputItem) {
                qreal currentX = outputItem->x();
                qreal currentY = outputItem->y();
                bool shouldPreservePosition = (state.x == 0 && state.y == 0) &&
                                             (currentX != 0 || currentY != 0);

                if (!shouldPreservePosition) {
                    outputItem->setX(state.x);
                    outputItem->setY(state.y);
                }
            }
        }

        auto outputHelper = renderWindow->getOutputHelper(viewport);
        if (!outputHelper) {
            qCWarning(treelandCore) << "No output helper for viewport" << viewport;
            m_outputManager->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        WOutputHelper::ExtraState extraState;
        wlr_output_state_set_enabled(extraState.get(), state.enabled);

        // Only set mode/scale/transform properties when enabling output
        // Reason: wlroots doesn't allow setting these properties on disabled outputs
        // When user disables output and changes properties:
        //   1. Properties are saved in QSettings (see onOutputCommitFinished)
        //   2. When re-enabling, properties are loaded from QSettings and applied here
        if (state.enabled) {
            if (state.mode) {
                wlr_output_state_set_mode(extraState.get(), state.mode);
            } else {
                wlr_output_state_set_custom_mode(extraState.get(),
                                                 state.customModeSize.width(),
                                                 state.customModeSize.height(),
                                                 state.customModeRefresh);
            }

            wlr_output_state_set_scale(extraState.get(), state.scale);
            wlr_output_state_set_transform(extraState.get(),
                                          static_cast<wl_output_transform>(state.transform));
            wlr_output_state_set_adaptive_sync_enabled(extraState.get(), state.adaptiveSyncEnabled);

            if (auto outputItem = qobject_cast<WOutputItem*>(viewport->parentItem())) {
                QMetaObject::invokeMethod(outputItem, "setTransform",
                    Q_ARG(QVariant, QVariant::fromValue(static_cast<WOutput::Transform>(state.transform))));
            }
        }

        if (!outputHelper->setExtraState(extraState)) {
            qCWarning(treelandCore) << "Failed to set extra state for output" << state.output->name();
            m_outputManager->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }
        auto config = m_pendingOutputConfig.config;
        QPointer<Helper> self(this);
        outputHelper->scheduleCommitJob(
            [self, config, extraState, renderWindow, viewport](bool success, WOutputHelper::ExtraState committedState) {
                if (!self) {
                    return;
                }

                if (committedState == extraState) {
                    self->onOutputCommitFinished(config, success);
                    if (success && committedState) {
                        bool wasStateOnlyCommit = (committedState->committed & (WLR_OUTPUT_STATE_MODE |
                                                                                WLR_OUTPUT_STATE_SCALE |
                                                                                WLR_OUTPUT_STATE_TRANSFORM |
                                                                                WLR_OUTPUT_STATE_ENABLED)) &&
                                                  !(committedState->committed & WLR_OUTPUT_STATE_BUFFER);
                        bool isDisable = (committedState->committed & WLR_OUTPUT_STATE_ENABLED) && !committedState->enabled;
                        if (wasStateOnlyCommit && !isDisable) {
                            renderWindow->update(viewport);
                        }
                    }
                } else {
                    qCWarning(treelandCore) << "Commit callback received unexpected state pointer!"
                                            << "Expected:" << extraState.get()
                                            << "Got:" << committedState.get();
                    self->onOutputCommitFinished(config, false);
                }
            },
            WOutputHelper::AfterCommitStage
        );
        m_pendingOutputConfig.pendingCommits++;
        renderWindow->update(viewport);

        // Special handling for disabled → enabled transition
        // wlroots doesn't send frame events for disabled outputs,
        // so we need to force render to trigger the commit
        if (state.enabled && !state.output->isEnabled()) {
            renderWindow->render(viewport, true);
        }
    }
}

void Helper::onOutputCommitFinished(qw_output_configuration_v1 *config, bool success)
{
    if (!config) {
        return;
    }

    if (config != m_pendingOutputConfig.config) {
        return;
    }

    if (!success) {
        m_pendingOutputConfig.allSuccess = false;
    }

    m_pendingOutputConfig.pendingCommits--;
    if (m_pendingOutputConfig.pendingCommits == 0) {
        bool ok = m_pendingOutputConfig.allSuccess;
        if (ok) {
            // TODO: Replace QSettings with DConfig to support customization
            // and avoid IO operations in main thread
            QString cache_location = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            QSettings settings(cache_location + "/output.ini", QSettings::IniFormat);
            for (const WOutputState &state : std::as_const(m_pendingOutputConfig.states)) {
                // Only save configuration for enabled outputs
                // Reason: When disabled, mode/scale/transform are not committed to wlroots,
                // so we should not save uncommitted values
                if (!state.enabled) {
                    qCDebug(treelandCore) << "Skipping config save for disabled output:" << state.output->name();
                    continue;
                }

                settings.beginGroup(QString("output.%1").arg(state.output->name()));
                settings.setValue("width", state.mode ? state.mode->width : state.customModeSize.width());
                settings.setValue("height", state.mode ? state.mode->height : state.customModeSize.height());
                settings.setValue("refresh", state.mode ? state.mode->refresh : state.customModeRefresh);
                settings.setValue("transform", state.transform);
                settings.setValue("scale", state.scale);
                settings.setValue("adaptiveSyncEnabled", state.adaptiveSyncEnabled);
                settings.endGroup();
            }
        }
        m_outputManager->sendResult(config, ok);
        m_pendingOutputConfig = {};
    }
}

void Helper::onSetOutputPowerMode(wlr_output_power_v1_set_mode_event *event)
{
    auto output = qw_output::from(event->output);
    qw_output_state newState;

    switch (event->mode) {
    case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
        if (!output->handle()->enabled) {
            return;
        }
        newState.set_enabled(false);
        output->commit_state(newState);
        break;
    case ZWLR_OUTPUT_POWER_V1_MODE_ON:
        if (output->handle()->enabled) {
            return;
        }
        newState.set_enabled(true);
        output->commit_state(newState);
        break;
    }
}

void Helper::onNewIdleInhibitor(wlr_idle_inhibitor_v1 *wlr_inhibitor)
{
    auto inhibitor = qw_idle_inhibitor_v1::from(wlr_inhibitor);
    m_idleInhibitors.append(inhibitor);

    connect(inhibitor, &qw_idle_inhibitor_v1::before_destroy, this, [this, inhibitor]() {
        m_idleInhibitors.removeOne(inhibitor);
        updateIdleInhibitor();
    });

    auto wsurface = WSurface::fromHandle(wlr_inhibitor->surface);
    connect(wsurface, &WSurface::mappedChanged, inhibitor, [this]() {
        updateIdleInhibitor();
    });

    auto toplevel = WXdgToplevelSurface::fromSurface(wsurface);
    if (toplevel) {
        connect(toplevel, &WXdgToplevelSurface::minimizeChanged, inhibitor, [this]() {
            updateIdleInhibitor();
        });
    }

    updateIdleInhibitor();
}

void Helper::updateIdleInhibitor()
{
    if (m_screensaverInterfaceV1->isInhibited()) {
        m_idleNotifier->set_inhibited(true);
        return;
    }
    for (const auto &inhibitor : std::as_const(m_idleInhibitors)) {
        auto wsurface = WSurface::fromHandle((*inhibitor)->surface);
        bool visible = wsurface->mapped();
        auto toplevel = WXdgToplevelSurface::fromSurface(wsurface);
        if (toplevel)
            visible &= !toplevel->isMinimized();

        if (visible) {
            m_idleNotifier->set_inhibited(true);
            return;
        }
    }
    m_idleNotifier->set_inhibited(false);
}

void Helper::onDockPreview(std::vector<SurfaceWrapper *> surfaces,
                           WSurface *target,
                           QPoint pos,
                           ForeignToplevelV1::PreviewDirection direction)
{
    SurfaceWrapper *dockWrapper = m_rootSurfaceContainer->getSurface(target);
    Q_ASSERT(dockWrapper);

    QMetaObject::invokeMethod(m_dockPreview,
                              "show",
                              QVariant::fromValue(surfaces),
                              QVariant::fromValue(dockWrapper),
                              QVariant::fromValue(pos),
                              QVariant::fromValue(direction));
}

void Helper::onDockPreviewTooltip(QString tooltip,
                                  WSurface *target,
                                  QPoint pos,
                                  ForeignToplevelV1::PreviewDirection direction)
{
    SurfaceWrapper *dockWrapper = m_rootSurfaceContainer->getSurface(target);
    Q_ASSERT(dockWrapper);
    QMetaObject::invokeMethod(m_dockPreview,
                              "showTooltip",
                              QVariant::fromValue(tooltip),
                              QVariant::fromValue(dockWrapper),
                              QVariant::fromValue(pos),
                              QVariant::fromValue(direction));
}

void Helper::onShowDesktop()
{
    WindowManagementV1::DesktopState s = m_windowManagement->desktopState();
    if (m_showDesktop == s
        || (s != WindowManagementV1::DesktopState::Normal
            && s != WindowManagementV1::DesktopState::Show))
        return;

    m_showDesktop = s;
    const auto &surfaces = getWorkspaceSurfaces();
    for (auto &surface : surfaces) {
        if (surface->isMinimized()) {
            continue;
        }
        if (s == WindowManagementV1::DesktopState::Normal) {
            surface->startShowDesktopAnimation(true);
        } else if (s == WindowManagementV1::DesktopState::Show) {
            surface->startShowDesktopAnimation(false);
        }
    }
}

void Helper::onSetCopyOutput(treeland_virtual_output_v1 *virtual_output)
{
    Output *mirrorOutput = nullptr;
    for (Output *output : m_outputList) {
        if (!virtual_output->outputList.contains(output->output()->name())) {
            QString screen = output->output()->name() + " does not exist!";
            virtual_output->send_error(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_OUTPUT,
                                       screen.toLocal8Bit().data());
            return;
        }

        if (!output->isPrimary()) {
            QString screen =
                output->output()->name() + " is already a copy screen, invalid setting!";
            virtual_output->send_error(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_OUTPUT,
                                       screen.toLocal8Bit().data());
            return;
        }

        if (output->output()->name() == virtual_output->outputList.at(0))
            mirrorOutput = output;
    }

    for (int i = 0; i < m_outputList.size(); i++) {
        Output *currentOutput = m_outputList.at(i);
        if (currentOutput == mirrorOutput)
            continue;

        // When setting the primaryOutput as a copy screen, set the mirrorOutput
        // as the home screen.
        if (m_rootSurfaceContainer->primaryOutput() == currentOutput)
            m_rootSurfaceContainer->setPrimaryOutput(mirrorOutput);

        Output *o = createCopyOutput(currentOutput->output(), mirrorOutput);
        m_rootSurfaceContainer->removeOutput(currentOutput);
        currentOutput->deleteLater();
        m_outputList.replace(i, o);
    }

    m_mode = OutputMode::Copy;
    const auto &surfaces = getWorkspaceSurfaces();
    moveSurfacesToOutput(surfaces, mirrorOutput, nullptr);
}

void Helper::onRestoreCopyOutput(treeland_virtual_output_v1 *virtual_output)
{
    const QString targetName = virtual_output->outputList.at(0);
    if (!std::any_of(m_outputList.constBegin(), m_outputList.constEnd(),
                     [&targetName](const Output *output) { return output->output()->name() == targetName; })) {
        virtual_output->send_error(
            TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_OUTPUT,
            qPrintable(QString("Target output %1 does not exist!").arg(targetName))
        );
        return;
    }

    for (int i = 0; i < m_outputList.size(); i++) {
        Output *currentOutput = m_outputList.at(i);
        if (currentOutput->output()->name() == targetName)
            continue;

        Output *o = createNormalOutput(m_outputList.at(i)->output());
        o->enable();
        m_outputList.at(i)->deleteLater();
        m_outputList.replace(i, o);
    }
    m_mode = OutputMode::Extension;
}

void Helper::onSurfaceWrapperAdded(SurfaceWrapper *wrapper)
{
    bool isXdgToplevel = wrapper->type() == SurfaceWrapper::Type::XdgToplevel;
    bool isXdgPopup = wrapper->type() == SurfaceWrapper::Type::XdgPopup;
    bool isXwayland = wrapper->type() == SurfaceWrapper::Type::XWayland;
    bool isLayer = wrapper->type() == SurfaceWrapper::Type::Layer;

    connect(m_activeSession.lock().get(), &Session::aboutToBeDestroyed, wrapper, [this, wrapper]() {
        onSurfaceWrapperAboutToRemove(wrapper);
        m_rootSurfaceContainer->destroyForSurface(wrapper);
    });

    if (isXdgToplevel || isXdgPopup || isLayer) {
        auto *attached =
            new Personalization(wrapper->shellSurface(), m_personalization, wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                attached, &Personalization::deleteLater);

        auto updateNoTitlebar = [this, attached] {
            auto wrapper = attached->surfaceWrapper();
            if (attached->noTitlebar()) {
                wrapper->setNoTitleBar(true);
                auto layer = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
                if (!isLaunchpad(layer)) {
                    wrapper->setNoDecoration(false);
                }
                return;
            }

            wrapper->resetNoTitleBar();
            wrapper->setNoDecoration(m_xdgDecorationManager->modeBySurface(wrapper->surface())
                                     != WXdgDecorationManager::Server);
        };

        if (isXdgToplevel) {
            connect(
                m_xdgDecorationManager,
                &WXdgDecorationManager::surfaceModeChanged,
                attached,
                [attached, updateNoTitlebar](
                    WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                    [[maybe_unused]] Waylib::Server::WXdgDecorationManager::DecorationMode mode) {
                    if (surface == attached->surfaceWrapper()->surface()) {
                        updateNoTitlebar();
                    }
                });
        }

        connect(attached, &Personalization::windowStateChanged, this, updateNoTitlebar);
        updateNoTitlebar();

        auto updateBlur = [attached] {
            attached->surfaceWrapper()->setBlur(attached->backgroundType() == Personalization::BackgroundType::Blur);
        };
        connect(attached, &Personalization::backgroundTypeChanged, this, updateBlur);
        updateBlur();
        if (isLayer) {
            auto layer = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
            if (isLaunchpad(layer))
                wrapper->setCoverEnabled(true);
        }
    }

    if (isXwayland) {
        auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
        auto updateDecorationTitleBar = [this, wrapper, xwaylandSurface]() {
            auto *xwayland = xwaylandSurface->xwayland();
            xcb_connection_t *connection = xwayland ? xwayland->xcbConnection() : nullptr;
            xcb_atom_t atom;
            if (xwayland) {
                if (auto session = sessionForXWayland(xwayland))
                    atom = session->noTitlebarAtom;
                else
                    atom = XCB_ATOM_NONE;
            } else {
                atom = XCB_ATOM_NONE;
            }
            if (!xwaylandSurface->isBypassManager()) {
                if (atom && connection
                    && !readWindowProperty(connection,
                                           xwaylandSurface->handle()->handle()->window_id,
                                           atom,
                                           XCB_ATOM_CARDINAL)
                            .isEmpty()) {
                    wrapper->setNoTitleBar(true);
                } else {
                    wrapper->setNoTitleBar(xwaylandSurface->decorationsFlags()
                                           & WXWaylandSurface::DecorationsNoTitle);
                }
                wrapper->setNoDecoration(xwaylandSurface->decorationsFlags()
                                         & WXWaylandSurface::DecorationsNoBorder);
            } else {
                wrapper->setNoTitleBar(true);
                wrapper->setNoDecoration(true);
            }
        };
        // When x11 surface dissociate, SurfaceWrapper will be destroyed immediately
        // but WXWaylandSurface will not, so must connect to `wrapper`
        xwaylandSurface->safeConnect(&WXWaylandSurface::bypassManagerChanged,
                                     wrapper,
                                     updateDecorationTitleBar);
        xwaylandSurface->safeConnect(&WXWaylandSurface::decorationsFlagsChanged,
                                     wrapper,
                                     updateDecorationTitleBar);
        updateDecorationTitleBar();

        wrapper->setHideByWorkspace(!surfaceBelongsToCurrentSession(wrapper));
    }

    if (!isLayer) {
        [[maybe_unused]] auto windowOverlapChecker = new WindowOverlapChecker(wrapper, wrapper);
    }

#ifndef DISABLE_DDM
    if (isLayer) {
        connect(this, &Helper::currentModeChanged, wrapper, [this, wrapper] {
            wrapper->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
        });
        wrapper->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
    }
#endif

    if (!wrapper->skipDockPreView()) {
        m_foreignToplevel->addSurface(wrapper->shellSurface());
        m_extForeignToplevelListV1->addSurface(wrapper->shellSurface());
        m_treelandForeignToplevel->addSurface(wrapper);
    }
    connect(wrapper, &SurfaceWrapper::skipDockPreViewChanged, this, [this, wrapper] {
        if (wrapper->skipDockPreView()) {
            m_foreignToplevel->removeSurface(wrapper->shellSurface());
            m_extForeignToplevelListV1->removeSurface(wrapper->shellSurface());
            m_treelandForeignToplevel->removeSurface(wrapper);
        } else {
            m_foreignToplevel->addSurface(wrapper->shellSurface());
            m_extForeignToplevelListV1->addSurface(wrapper->shellSurface());
            m_treelandForeignToplevel->addSurface(wrapper);
        }
    });
}

void Helper::onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper)
{
    if (!wrapper->skipDockPreView()) {
        m_foreignToplevel->removeSurface(wrapper->shellSurface());
        m_extForeignToplevelListV1->removeSurface(wrapper->shellSurface());
        m_treelandForeignToplevel->removeSurface(wrapper);
    }
}

bool Helper::surfaceBelongsToCurrentSession(SurfaceWrapper *wrapper)
{
    WClient *client = wrapper->surface()->waylandClient();
    WSocket *socket = client ? client->socket()->rootSocket() : nullptr;
    return socket && socket->isEnabled();
}

void Helper::deleteTaskSwitch()
{
    if (m_taskSwitch) {
        m_taskSwitch->deleteLater();
        m_taskSwitch = nullptr;
    }
}

void Helper::init(Treeland::Treeland *treeland)
{
    m_treeland = treeland;
    auto engine = qmlEngine();
    m_userModel = engine->singletonInstance<UserModel *>("Treeland", "UserModel");

    engine->rootContext()->setContextProperty("TreelandConfig", m_config);

    engine->setContextForObject(m_renderWindow, engine->rootContext());
    engine->setContextForObject(m_renderWindow->contentItem(), engine->rootContext());
    m_rootSurfaceContainer->setQmlEngine(engine);
    m_rootSurfaceContainer->init(m_server);

    m_seat = m_server->attach<WSeat>();
    m_seat->setEventFilter(this);
    m_seat->setCursor(m_rootSurfaceContainer->cursor());
    m_seat->setKeyboardFocusWindow(m_renderWindow);

    connect(m_seat, &WSeat::requestDrag, this, &Helper::handleRequestDrag);

    m_backend = m_server->attach<WBackend>();
    connect(m_backend, &WBackend::inputAdded, this, [this](WInputDevice *device) {
        m_seat->attachInputDevice(device);
        if (InputDevice::instance()->initTouchPad(device)) {
            if (m_windowGesture) {
                m_windowGesture->addTouchpadSwipeGesture(SwipeGesture::Up, 3);
                m_windowGesture->addTouchpadHoldGesture(3);
            }

            if (m_multiTaskViewGesture) {
                m_multiTaskViewGesture->addTouchpadSwipeGesture(SwipeGesture::Up, 4);
                m_multiTaskViewGesture->addTouchpadSwipeGesture(SwipeGesture::Right, 4);
            }
        }
    });

    connect(m_backend, &WBackend::inputRemoved, this, [this](WInputDevice *device) {
        m_seat->detachInputDevice(device);
    });

    m_ddmInterfaceV1 = m_server->attach<DDMInterfaceV1>();

    m_outputManager = m_server->attach<WOutputManagerV1>();
    connect(m_backend, &WBackend::outputAdded, this, &Helper::onOutputAdded);
    connect(m_backend, &WBackend::outputRemoved, this, &Helper::onOutputRemoved);

    m_ddeShellV1 = m_server->attach<DDEShellManagerInterfaceV1>();
    connect(m_ddeShellV1, &DDEShellManagerInterfaceV1::toggleMultitaskview, this, [this] {
        if (m_multitaskView) {
            m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        }
    });
    connect(m_ddeShellV1,
            &DDEShellManagerInterfaceV1::requestPickWindow,
            this,
            &Helper::handleWindowPicker);
    connect(m_ddeShellV1,
            &DDEShellManagerInterfaceV1::lockScreenCreated,
            this,
            &Helper::handleLockScreen);
    m_shellHandler->createComponent(engine);
    m_shellHandler->initXdgShell(m_server);
    m_shellHandler->initLayerShell(m_server);
    m_shellHandler->initInputMethodHelper(m_server, m_seat);

    m_foreignToplevel = m_server->attach<WForeignToplevel>();
    m_extForeignToplevelListV1 = m_server->attach<WExtForeignToplevelListV1>();
    m_treelandForeignToplevel = m_server->attach<ForeignToplevelV1>();
    Q_ASSERT(m_treelandForeignToplevel);
    qmlRegisterSingletonInstance<ForeignToplevelV1>("Treeland.Protocols",
                                                    1,
                                                    0,
                                                    "ForeignToplevelV1",
                                                    m_treelandForeignToplevel);
    qRegisterMetaType<ForeignToplevelV1::PreviewDirection>();

    connect(m_shellHandler,
            &ShellHandler::surfaceWrapperAdded,
            this,
            &Helper::onSurfaceWrapperAdded);

    connect(m_shellHandler,
            &ShellHandler::surfaceWrapperAboutToRemove,
            this,
            &Helper::onSurfaceWrapperAboutToRemove);

    auto *xdgOutputManager =
        m_server->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());

    m_outputManagerV1 = m_server->attach<OutputManagerV1>();
    m_wallpaperColorV1 = m_server->attach<WallpaperColorV1>();
    m_windowManagement = m_server->attach<WindowManagementV1>();
    m_virtualOutput = m_server->attach<VirtualOutputV1>();
    m_shortcut = m_server->attach<ShortcutV1>();
    auto captureManagerV1 = m_server->attach<CaptureManagerV1>();
    captureManagerV1->setOutputRenderWindow(m_renderWindow);

    connect(
        captureManagerV1,
        &CaptureManagerV1::contextInSelectionChanged,
        this,
        [this, captureManagerV1] {
            if (captureManagerV1->contextInSelection()) {
                m_captureSelector = qobject_cast<CaptureSourceSelector *>(
                    qmlEngine()->createCaptureSelector(m_rootSurfaceContainer, captureManagerV1));
            } else if (m_captureSelector) {
                m_captureSelector->deleteLater();
            }
        });
    m_personalization = m_server->attach<PersonalizationV1>();

    auto updateCurrentUser = [this] {
        m_config = TreelandConfig::createByName("org.deepin.treeland",
                                                "org.deepin.treeland",
                                                "/" + m_userModel->currentUserName());
        auto user = m_userModel->currentUser();
        m_personalization->setUserId(user ? user->UID() : getuid());
    };
    connect(m_userModel, &UserModel::currentUserNameChanged, this, updateCurrentUser);

    updateCurrentUser();

    connect(m_personalization,
            &PersonalizationV1::backgroundChanged,
            this,
            [this](const QString &output, bool isdark) {
                m_wallpaperColorV1->updateWallpaperColor(output, isdark);
            });

    for (auto output : m_rootSurfaceContainer->outputs()) {
        const QString &outputName = output->output()->name();
        m_wallpaperColorV1->updateWallpaperColor(outputName,
                                                 m_personalization->backgroundIsDark(outputName));
    }

    connect(m_windowManagement,
            &WindowManagementV1::desktopStateChanged,
            this,
            &Helper::onShowDesktop);

    connect(m_virtualOutput,
            &VirtualOutputV1::requestCreateVirtualOutput,
            this,
            &Helper::onSetCopyOutput);

    connect(m_virtualOutput,
            &VirtualOutputV1::destroyVirtualOutput,
            this,
            &Helper::onRestoreCopyOutput);

    connect(m_outputManagerV1,
            &OutputManagerV1::requestSetPrimaryOutput,
            this,
            [this](const char *name) {
                for (auto &&output : m_rootSurfaceContainer->outputs()) {
                    if (strcmp(output->output()->nativeHandle()->name, name) == 0) {
                        m_rootSurfaceContainer->setPrimaryOutput(output);
                    }
                }
            });

    connect(m_rootSurfaceContainer, &RootSurfaceContainer::primaryOutputChanged, this, [this]() {
        if (m_rootSurfaceContainer->primaryOutput()) {
            m_outputManagerV1->sendPrimaryOutput(
                m_rootSurfaceContainer->primaryOutput()->output()->nativeHandle()->name);
            if (m_lockScreen) {
                m_lockScreen->setPrimaryOutputName(m_rootSurfaceContainer->primaryOutput()->output()->name());
            }
        }
    });

    qmlRegisterUncreatableType<Personalization>("Treeland.Protocols",
                                                1,
                                                0,
                                                "Personalization",
                                                "Only for Enum");

    qmlRegisterUncreatableType<DDEShellHelper>("Treeland.Protocols",
                                               1,
                                               0,
                                               "DDEShellHelper",
                                               "Only for attached");
    qmlRegisterUncreatableType<CaptureSource>("Treeland.Protocols",
                                              1,
                                              0,
                                              "CaptureSource",
                                              "An abstract class");
    qmlRegisterType<CaptureContextV1>("Treeland.Protocols", 1, 0, "CaptureContextV1");
    qmlRegisterType<CaptureSourceSelector>("Treeland.Protocols", 1, 0, "CaptureSourceSelector");

    m_server->attach<WSecurityContextManager>();
    m_server->start();
    m_renderer = WRenderHelper::createRenderer(m_backend->handle());
    if (!m_renderer) {
        qCFatal(treelandCore) << "Failed to create renderer";
    }

    m_allocator = qw_allocator::autocreate(*m_backend->handle(), *m_renderer);
    m_renderer->init_wl_display(*m_server->handle());
    qw_drm::create(*m_server->handle(), *m_renderer);

    // free follow display
    m_compositor = qw_compositor::create(*m_server->handle(), 6, *m_renderer);
    qw_subcompositor::create(*m_server->handle());
    qw_screencopy_manager_v1::create(*m_server->handle());
    qw_ext_image_copy_capture_manager_v1::create(*m_server->handle(), 1);
    qw_ext_output_image_capture_source_manager_v1::create(*m_server->handle(), 1);
    m_foreignToplevelImageCaptureManager = qw_ext_foreign_toplevel_image_capture_source_manager_v1::create(*m_server->handle(), 1);
    connect(m_foreignToplevelImageCaptureManager,
            &qw_ext_foreign_toplevel_image_capture_source_manager_v1::notify_new_request,
            this, &Helper::handleNewForeignToplevelCaptureRequest);

    qw_viewporter::create(*m_server->handle());
    m_renderWindow->init(m_renderer, m_allocator);

    auto *xwaylandOutputManager =
        m_server->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());
    xwaylandOutputManager->setScaleOverride(1.0);
    xdgOutputManager->setFilter([this](WClient *client) { return !isXWaylandClient(client); });
    xwaylandOutputManager->setFilter([this](WClient *client) { return isXWaylandClient(client); });
    // User dde does not has a real Logind session, so just pass 0 as id
    updateActiveUserSession(QStringLiteral("dde"), 0);
    connect(m_userModel, &UserModel::userLoggedIn, this, &Helper::updateActiveUserSession);
    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();
    connect(m_xdgDecorationManager,
            &WXdgDecorationManager::surfaceModeChanged,
            this,
            &Helper::onSurfaceModeChanged);

    auto gammaControlManager = qw_gamma_control_manager_v1::create(*m_server->handle());
    connect(gammaControlManager,
            &qw_gamma_control_manager_v1::notify_set_gamma,
            this,
            &Helper::setGamma);

    connect(m_outputManager,
            &WOutputManagerV1::requestTestOrApply,
            this,
            &Helper::onOutputTestOrApply);

    m_server->attach<WCursorShapeManagerV1>();
    qw_fractional_scale_manager_v1::create(*m_server->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);
    qw_data_control_manager_v1::create(*m_server->handle());
    qw_ext_data_control_manager_v1::create(*m_server->handle(), EXT_DATA_CONTROL_MANAGER_V1_VERSION);
    qw_alpha_modifier_v1::create(*m_server->handle());

    m_dockPreview = engine->createDockPreview(m_renderWindow->contentItem());

    connect(m_treelandForeignToplevel,
            &ForeignToplevelV1::requestDockPreview,
            this,
            &Helper::onDockPreview);
    connect(m_treelandForeignToplevel,
            &ForeignToplevelV1::requestDockPreviewTooltip,
            this,
            &Helper::onDockPreviewTooltip);

    connect(m_treelandForeignToplevel,
            &ForeignToplevelV1::requestDockClose,
            m_dockPreview,
            [this]() {
                QMetaObject::invokeMethod(m_dockPreview, "close");
            });


    m_idleNotifier = qw_idle_notifier_v1::create(*m_server->handle());

    m_idleInhibitManager = qw_idle_inhibit_manager_v1::create(*m_server->handle());
    connect(m_idleInhibitManager, &qw_idle_inhibit_manager_v1::notify_new_inhibitor, this, &Helper::onNewIdleInhibitor);

    m_screensaverInterfaceV1 = m_server->attach<ScreensaverInterfaceV1>();

    m_outputPowerManager = qw_output_power_manager_v1::create(*m_server->handle());

    connect(m_outputPowerManager, &qw_output_power_manager_v1::notify_set_mode, this, &Helper::onSetOutputPowerMode);
#ifdef EXT_SESSION_LOCK_V1
    m_sessionLockManager = m_server->attach<WSessionLockManager>();
    if (!m_lockScreen) {
        setLockScreenImpl(nullptr);
    }
    connect(m_sessionLockManager,
            &WSessionLockManager::lockCreated,
            this,
            &Helper::onExtSessionLock);
#endif
    m_backend->handle()->start();
}

bool Helper::socketEnabled() const
{
    auto ptr = m_activeSession.lock();
    if (ptr && ptr->socket)
        return ptr->socket->isEnabled();
    return false;
}

void Helper::setSocketEnabled(bool newEnabled)
{
    auto ptr = m_activeSession.lock();
    if (ptr && ptr->socket)
        ptr->socket->setEnabled(newEnabled);
    else
        qCWarning(treelandCore) << "Can't set enabled for empty socket!";
}

void Helper::activateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason)
{
    if (m_blockActivateSurface && wrapper && wrapper->type() != SurfaceWrapper::Type::LockScreen) {
        if (wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Activate)) {
            workspace()->pushActivedSurface(wrapper);
        }
        return;
    }
    if (!wrapper
        || !wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Activate)) {
        if (!wrapper)
            setActivatedSurface(nullptr);
        // else if wrapper don't have Activate Capability, do nothing
        // Otherwise, when click the dock, the last activate application will immediately
        // lose focus, and The dock will reactivate it instead of minimizing it
    } else {
        if (wrapper->hasActiveCapability()) {
            setActivatedSurface(wrapper);
        } else {
            qCCritical(treelandShell) << "Trying to activate a surface which doesn't have ActiveCapability!";
        }
    }

    if (!wrapper
        || (wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus)
            && wrapper->acceptKeyboardFocus()))
        requestKeyboardFocusForSurface(wrapper, reason);
}

void Helper::forceActivateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason)
{
    if (!wrapper) {
        qCCritical(treelandShell) << "Don't force activate to empty surface! do you want `Helper::activeSurface(nullptr)`?";
        return;
    }
    if (!wrapper->shellSurface()) {
        qCWarning(treelandShell) << "Try to force activate a destroyed surface!";
        return;
    }

    restoreFromShowDesktop(wrapper);

    if (wrapper->isMinimized()) {
        wrapper->requestCancelMinimize(
            !(reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason));
    }

    if (!wrapper->surface()->mapped()) {
        qCWarning(treelandShell) << "Can't activate unmapped surface: " << wrapper;
        return;
    }

    if (!wrapper->showOnWorkspace(workspace()->current()->id()))
        workspace()->switchTo(workspace()->modelIndexOfSurface(wrapper));
    Helper::instance()->activateSurface(wrapper, reason);
}

RootSurfaceContainer *Helper::rootContainer() const
{
    return m_rootSurfaceContainer;
}

void Helper::fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface)
{
    auto position = surface->geometry().bottomRight();
    m_fakelastPressedPosition = position;
    m_seat->setCursorPosition(position);
    Q_EMIT surface->requestResize(Qt::BottomEdge | Qt::RightEdge);
}

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *, QInputEvent *event)
{
    if (event->isInputEvent()) {
        m_idleNotifier->notify_activity(seat->nativeHandle());
    }
    // NOTE: Unable to distinguish meta from other key combinations
    //       For example, Meta+S will still receive Meta release after
    //       fully releasing the key, actively detect whether there are
    //       other keys, and reset the state.
    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);
        switch (kevent->key()) {
        case Qt::Key_Meta:
        case Qt::Key_Super_L:
            m_singleMetaKeyPendingPressed = true;
            break;
        default:
            m_singleMetaKeyPendingPressed = false;
            break;
        }
    }

    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);

        // The debug view shortcut should always handled first
        if (QKeySequence(kevent->keyCombination())
            == QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier | Qt::Key_F11)) {
            if (toggleDebugMenuBar())
                return true;
        }

        // Switch TTY with Ctrl + Alt + F1-F12
        if (kevent->modifiers() == (Qt::ControlModifier | Qt::AltModifier)) {
            auto key = kevent->key();
            // We don't call libseat_disable_seat after switching TTY by
            // calling DDM, which will cause the keyboard stuck in current
            // state (Ctrl + Alt + Fx), and send switchToVt repeatly.
            // Check if the backend is active to avoid this.
            if (key >= Qt::Key_F1 && key <= Qt::Key_F12 && m_backend->isSessionActive()) {
                const int vtnr = key - Qt::Key_F1 + 1;
                if (m_ddmInterfaceV1 && m_ddmInterfaceV1->isConnected()) {
                    m_ddmInterfaceV1->switchToVt(vtnr);
                } else {
                    qCDebug(treelandCore) << "DDM is not connected";
                    showLockScreen(false);
                    m_backend->session()->change_vt(vtnr);
                }
                return true;
            }
        }

        if (m_lockScreen && m_lockScreen->available()
            && m_currentMode == CurrentMode::Normal
            && QKeySequence(kevent->modifiers() | kevent->key())
                == QKeySequence(Qt::ControlModifier | Qt::AltModifier | Qt::Key_Delete)) {
            setCurrentMode(CurrentMode::LockScreen);
            m_lockScreen->shutdown();
            setWorkspaceVisible(false);
            return true;
        }
        if (QKeySequence(kevent->modifiers() | kevent->key())
            == QKeySequence(Qt::META | Qt::Key_F12)) {
            Q_EMIT requestQuit();
            return true;
        } else if (QKeySequence(kevent->modifiers() | kevent->key())
            == QKeySequence(Qt::META | Qt::Key_F11)) {
            toggleFpsDisplay();
            return true;
        } else if (m_captureSelector) {
            if (event->modifiers() == Qt::NoModifier && kevent->key() == Qt::Key_Escape)
                m_captureSelector->cancelSelection();
        } else if (event->modifiers() == Qt::MetaModifier) {
            const QList<Qt::Key> switchWorkspaceNums = { Qt::Key_1, Qt::Key_2, Qt::Key_3,
                                                         Qt::Key_4, Qt::Key_5, Qt::Key_6 };
            if (kevent->key() == Qt::Key_Right) {
                restoreFromShowDesktop();
                workspace()->switchToNext();
                return true;
            } else if (kevent->key() == Qt::Key_Left) {
                restoreFromShowDesktop();
                workspace()->switchToPrev();
                return true;
            } else if (switchWorkspaceNums.contains(kevent->key())) {
                restoreFromShowDesktop();
                workspace()->switchTo(switchWorkspaceNums.indexOf(kevent->key()));
                return true;
            } else if (kevent->key() == Qt::Key_S
                       && (m_currentMode == CurrentMode::Normal
                           || m_currentMode == CurrentMode::Multitaskview)) {
                restoreFromShowDesktop();
                if (m_multitaskView) {
                    m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
                }
                return true;
#ifndef DISABLE_DDM
            } else if (m_lockScreen && m_lockScreen->available() && kevent->key() == Qt::Key_L) {
                if (m_lockScreen->isLocked()) {
                    return true;
                }

                showLockScreen();
                return true;
#endif
            } else if (kevent->key() == Qt::Key_D) { // ShowDesktop : Meta + D
                if (m_currentMode == CurrentMode::Multitaskview) {
                    return true;
                }
                if (m_showDesktop == WindowManagementV1::DesktopState::Normal)
                    m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Show);
                else if (m_showDesktop == WindowManagementV1::DesktopState::Show)
                    m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Normal);
                return true;
            } else if (kevent->key() == Qt::Key_Up && m_activatedSurface) { // maximize: Meta + up
                m_activatedSurface->requestMaximize();
                return true;
            } else if (kevent->key() == Qt::Key_Down
                       && m_activatedSurface) { // cancelMaximize : Meta + down
                m_activatedSurface->requestCancelMaximize();
                return true;
            }
        } else if (kevent->key() == Qt::Key_Alt) {
            m_taskAltTimestamp = kevent->timestamp();
            m_taskAltCount = 0;
        } else if ((m_currentMode == CurrentMode::Normal
                    || m_currentMode == CurrentMode::WindowSwitch)
                   && (kevent->key() == Qt::Key_Tab || kevent->key() == Qt::Key_Backtab
                       || kevent->key() == Qt::Key_QuoteLeft
                       || kevent->key() == Qt::Key_AsciiTilde)) {
            if (event->modifiers().testFlag(Qt::AltModifier)) {
                int detal = kevent->timestamp() - m_taskAltTimestamp;
                if (detal < 150 && !kevent->isAutoRepeat()) {
                    auto current = Helper::instance()->workspace()->current();
                    Q_ASSERT(current);
                    auto next_surface = current->findNextActivedSurface();
                    if (next_surface)
                        Helper::instance()->forceActivateSurface(next_surface, Qt::TabFocusReason);
                    return true;
                }

                if (m_taskSwitch.isNull()) {
                    auto contentItem = window()->contentItem();
                    auto output = rootContainer()->primaryOutput();
                    m_taskSwitch = qmlEngine()->createTaskSwitcher(output, contentItem);

                    // Restore the real state of the window when Task Switche
                    restoreFromShowDesktop();
                    connect(m_taskSwitch,
                            SIGNAL(switchOnChanged()),
                            this,
                            SLOT(deleteTaskSwitch()));
                    m_taskSwitch->setZ(RootSurfaceContainer::OverlayZOrder);
                }

                if (kevent->isAutoRepeat()) {
                    m_taskAltCount++;
                } else {
                    m_taskAltCount = 3;
                }

                if (m_taskAltCount >= 3) {
                    m_taskAltCount = 0;
                    setCurrentMode(CurrentMode::WindowSwitch);
                    QString appid;
                    if (kevent->key() == Qt::Key_QuoteLeft || kevent->key() == Qt::Key_AsciiTilde) {
                        auto surface = Helper::instance()->activatedSurface();
                        if (surface) {
                            appid = surface->shellSurface()->appId();
                        }
                    }
                    auto filter = Helper::instance()->workspace()->currentFilter();
                    filter->setFilterAppId(appid);

                    if (event->modifiers() == Qt::AltModifier) {
                        QMetaObject::invokeMethod(m_taskSwitch, "next");
                        return true;
                    } else if (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)
                               || event->modifiers()
                                   == (Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier)) {
                        QMetaObject::invokeMethod(m_taskSwitch, "previous");
                        return true;
                    }
                }
            }
        } else if (event->modifiers() == Qt::AltModifier) {
            if (kevent->key() == Qt::Key_F4 && m_activatedSurface) { // close window : Alt + F4
                m_activatedSurface->requestClose();
                return true;
            }
            if (kevent->key() == Qt::Key_Space && m_activatedSurface) {
                Q_EMIT m_activatedSurface->requestShowWindowMenu({ 0, 0 });
                return true;
            }
            if (m_taskSwitch) {
                if (kevent->key() == Qt::Key_Left) {
                    QMetaObject::invokeMethod(m_taskSwitch, "previous");
                    return true;
                } else if (kevent->key() == Qt::Key_Right) {
                    QMetaObject::invokeMethod(m_taskSwitch, "next");
                    return true;
                }
            }
        }
    }

    if (event->type() == QEvent::KeyRelease && !m_captureSelector) {
        auto kevent = static_cast<QKeyEvent *>(event);
        if (m_taskSwitch && m_taskSwitch->property("switchOn").toBool()) {
            if (kevent->key() == Qt::Key_Alt || kevent->key() == Qt::Key_Meta) {
                auto filter = Helper::instance()->workspace()->currentFilter();
                filter->setFilterAppId("");
                setCurrentMode(CurrentMode::Normal);
                QMetaObject::invokeMethod(m_taskSwitch, "exit");
            }
        }
    }

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
        handleLeftButtonStateChanged(event);
    }

    if (event->type() == QEvent::Wheel) {
        handleWhellValueChanged(event);
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    doGesture(event);

    if (auto surface = m_rootSurfaceContainer->moveResizeSurface()) {
        // for move resize
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            auto ownsOutput = surface->ownsOutput();
            if (!ownsOutput) {
                m_rootSurfaceContainer->endMoveResize();
                return false;
            }

            auto lastPosition =
                m_fakelastPressedPosition.value_or(cursor->lastPressedOrTouchDownPosition());
            auto increment_pos = ev->globalPosition() - lastPosition;
            m_rootSurfaceContainer->doMoveResize(increment_pos);

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            m_rootSurfaceContainer->endMoveResize();
            m_fakelastPressedPosition.reset();
        }
    }

    // handle shortcut
    if (!m_captureSelector && m_currentMode == CurrentMode::Normal
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        do {
            auto kevent = static_cast<QKeyEvent *>(event);
            // SKIP Meta+Meta
            if (kevent->key() == Qt::Key_Meta && kevent->modifiers() == Qt::NoModifier
                && !m_singleMetaKeyPendingPressed) {
                break;
            }
            bool isFind = false;
            QKeySequence sequence(kevent->modifiers() | kevent->key());
            auto user = m_userModel->currentUser();
            for (auto *action : m_shortcut->actions(user ? user->UID() : getuid())) {
                if (action->shortcut() == sequence) {
                    isFind = true;
                    if (event->type() == QEvent::KeyRelease) {
                        action->activate(QAction::Trigger);
                    }
                }
            }
            if (isFind) {
                return true;
            }
        } while (false);
    }

    return false;
}

bool Helper::afterHandleEvent([[maybe_unused]] WSeat *seat,
                              WSurface *watched,
                              QObject *surfaceItem,
                              QObject *,
                              QInputEvent *event)
{
    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto toplevelSurface = qobject_cast<WSurfaceItem *>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);

        auto surface = m_rootSurfaceContainer->getSurface(watched);
        activateSurface(surface, Qt::MouseFocusReason);
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (event->isSinglePointEvent()) {
        if (static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
            activateSurface(nullptr, Qt::OtherFocusReason);
        }
    }

    return false;
}

bool Helper::doGesture(QInputEvent *event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto e = static_cast<WGestureEvent *>(event);
        switch (e->gestureType()) {
        case Qt::BeginNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                InputDevice::instance()->processSwipeStart(e->fingerCount());

            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                InputDevice::instance()->processHoldStart(e->fingerCount());
            break;
        case Qt::EndNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture) {
                if (e->cancelled())
                    InputDevice::instance()->processSwipeCancel();
                else
                    InputDevice::instance()->processSwipeEnd();
            }
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                InputDevice::instance()->processHoldEnd();
            break;
        case Qt::PanNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                InputDevice::instance()->processSwipeUpdate(e->delta());
        case Qt::ZoomNativeGesture:
        case Qt::SmartZoomNativeGesture:
        case Qt::RotateNativeGesture:
        case Qt::SwipeNativeGesture:
        default:
            break;
        }
    }
    return false;
}

Output *Helper::createNormalOutput(WOutput *output)
{
    Output *o = Output::create(output, qmlEngine(), this);
    auto future = QtConcurrent::run([o, this]() {
        if (isNvidiaCardPresent()) {
            o->outputItem()->setProperty("forceSoftwareCursor", true);
        }
    });
    o->outputItem()->stackBefore(m_rootSurfaceContainer);
    m_rootSurfaceContainer->addOutput(o);
    return o;
}

Output *Helper::createCopyOutput(WOutput *output, Output *proxy)
{
    return Output::createCopy(output, proxy, qmlEngine(), this);
}

WOutputViewport *Helper::getOwnOutputViewport(WOutput *output)
{
    // Get the output's own viewport, not screenViewport()
    // In copy mode, screenViewport() returns the primary output's viewport,
    // but we need the OutputViewport that is a direct child of the OutputItem
    Output *outputObj = getOutput(output);
    if (!outputObj || !outputObj->outputItem()) {
        qCWarning(treelandCore) << "Invalid output object for" << output->name();
        return nullptr;
    }

    WOutputViewport *viewport = outputObj->outputItem()->findChild<WOutputViewport *>({}, Qt::FindDirectChildrenOnly);
    if (!viewport) {
        qCWarning(treelandCore) << "No viewport found for output" << output->name()
                                << "- OutputItem may not have been fully initialized";
    }
    return viewport;
}

QList<SurfaceWrapper *> Helper::getWorkspaceSurfaces(Output *filterOutput)
{
    QList<SurfaceWrapper *> surfaces;
    WOutputRenderWindow::paintOrderItemList(
        Helper::instance()->workspace(),
        [&surfaces, filterOutput](QQuickItem *item) -> bool {
            SurfaceWrapper *surfaceWrapper = qobject_cast<SurfaceWrapper *>(item);
            if (surfaceWrapper
                && (surfaceWrapper->showOnWorkspace(
                        Helper::instance()->workspace()->current()->id())
                    && (!filterOutput || surfaceWrapper->ownsOutput() == filterOutput))) {
                surfaces.append(surfaceWrapper);
                return true;
            } else {
                return false;
            }
        });

    return surfaces;
}

void Helper::moveSurfacesToOutput(const QList<SurfaceWrapper *> &surfaces,
                                  Output *targetOutput,
                                  Output *sourceOutput)
{
    if (!surfaces.isEmpty() && targetOutput) {
        const QRectF targetGeometry = targetOutput->geometry();

        for (auto *surface : surfaces) {
            if (!surface)
                continue;

            const QSizeF size = surface->size();
            QPointF newPos;

            if (surface->ownsOutput() == targetOutput) {
                newPos = surface->position();
            } else {
                const QRectF sourceGeometry =
                    sourceOutput ? sourceOutput->geometry() : surface->ownsOutput()->geometry();
                const QPointF relativePos = surface->position() - sourceGeometry.center();
                newPos = targetGeometry.center() + relativePos;
                surface->setOwnsOutput(targetOutput);
            }
            newPos.setX(
                qBound(targetGeometry.left(), newPos.x(), targetGeometry.right() - size.width()));
            newPos.setY(
                qBound(targetGeometry.top(), newPos.y(), targetGeometry.bottom() - size.height()));
            surface->setPosition(newPos);
        }
    }
}

SurfaceWrapper *Helper::keyboardFocusSurface() const
{
    auto item = m_renderWindow->activeFocusItem();
    if (!item)
        return nullptr;
    auto surface = qobject_cast<WSurfaceItem *>(item->parent());
    if (!surface)
        return nullptr;
    return qobject_cast<SurfaceWrapper *>(surface->parent());
}

void Helper::requestKeyboardFocusForSurface(SurfaceWrapper *newActivate, Qt::FocusReason reason)
{
    auto *nowKeyboardFocusSurface = keyboardFocusSurface();
    if (nowKeyboardFocusSurface == newActivate)
        return;

    Q_ASSERT(!newActivate
             || newActivate->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus));

    if (nowKeyboardFocusSurface && nowKeyboardFocusSurface->hasActiveCapability()) {
        if (newActivate) {
            if (nowKeyboardFocusSurface->shellSurface()->keyboardFocusPriority()
                > newActivate->shellSurface()->keyboardFocusPriority())
                return;
        } else {
            if (nowKeyboardFocusSurface->shellSurface()->keyboardFocusPriority() > 0)
                return;
        }
    }

    if (nowKeyboardFocusSurface)
        nowKeyboardFocusSurface->setFocus(false, reason);

    if (newActivate)
        newActivate->setFocus(true, reason);
}

SurfaceWrapper *Helper::activatedSurface() const
{
    return m_activatedSurface;
}

void Helper::setActivatedSurface(SurfaceWrapper *newActivateSurface)
{
    if (m_activatedSurface == newActivateSurface)
        return;

    if (newActivateSurface) {
        Q_ASSERT(newActivateSurface->showOnWorkspace(workspace()->current()->id()));
        newActivateSurface->stackToLast();
        if (newActivateSurface->type() == SurfaceWrapper::Type::XWayland) {
            auto xwaylandSurface =
                qobject_cast<WXWaylandSurface *>(newActivateSurface->shellSurface());
            xwaylandSurface->restack(nullptr, WXWaylandSurface::XCB_STACK_MODE_ABOVE);
        }
    }

    if (m_activatedSurface)
        m_activatedSurface->setActivate(false);

    if (newActivateSurface) {
        if (m_showDesktop == WindowManagementV1::DesktopState::Show) {
            m_showDesktop = WindowManagementV1::DesktopState::Normal;
            m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Normal);
            newActivateSurface->setHideByShowDesk(true);
        }

        newActivateSurface->setActivate(true);
        workspace()->pushActivedSurface(newActivateSurface);
    }
    m_activatedSurface = newActivateSurface;
    Q_EMIT activatedSurfaceChanged();
}

void Helper::setCursorPosition(const QPointF &position)
{
    m_rootSurfaceContainer->endMoveResize();
    m_seat->setCursorPosition(position);
}

void Helper::handleRequestDrag([[maybe_unused]] WSurface *surface)
{
    m_seat->setAlwaysUpdateHoverTarget(true);

    struct wlr_drag *drag = m_seat->nativeHandle()->drag;
    Q_ASSERT(drag);
    QObject::connect(qw_drag::from(drag), &qw_drag::notify_drop, this, [this] {
        if (m_ddeShellV1)
            DDEActiveInterface::sendDrop(m_seat);
    });

    QObject::connect(qw_drag::from(drag), &qw_drag::before_destroy, this, [this, drag] {
        drag->data = NULL;
        m_seat->setAlwaysUpdateHoverTarget(false);
    });

    if (m_ddeShellV1)
        DDEActiveInterface::sendStartDrag(m_seat);
}

void Helper::handleLockScreen(LockScreenInterface *lockScreen)
{
    connect(lockScreen, &LockScreenInterface::shutdown, this, [this]() {
        if (m_lockScreen && m_lockScreen->available() && currentMode() == Helper::CurrentMode::Normal) {
            setCurrentMode(CurrentMode::LockScreen);
            m_lockScreen->shutdown();
            setWorkspaceVisible(false);
        }
    });
    connect(lockScreen, &LockScreenInterface::lock, this, [this]() {
        if (m_lockScreen && m_lockScreen->available() && currentMode() == Helper::CurrentMode::Normal) {
            setCurrentMode(CurrentMode::LockScreen);
            m_lockScreen->lock();
            setWorkspaceVisible(false);
        }
    });
    connect(lockScreen, &LockScreenInterface::switchUser, this, [this]() {
        if (m_lockScreen && m_lockScreen->available() && currentMode() == Helper::CurrentMode::Normal) {
            setCurrentMode(CurrentMode::LockScreen);
            m_lockScreen->switchUser();
            setWorkspaceVisible(false);
        }
    });
}


void Helper::onSessionNew(const QString &sessionId, const QDBusObjectPath &sessionPath)
{
    const auto path = sessionPath.path();
    qCDebug(treelandCore) << "Session new, sessionId:" << sessionId << ", sessionPath:" << path;
    QDBusConnection::systemBus().connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Lock", this, SLOT(onSessionLock()));
    QDBusConnection::systemBus().connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Unlock", this, SLOT(onSessionUnLock()));
}

void Helper::onSessionLock()
{
    showLockScreen();
}

void Helper::onSessionUnlock()
{
    if (m_lockScreen) {
        m_lockScreen->unlock();
    }
}

void Helper::onExtSessionLock(WSessionLock *lock)
{
#ifdef EXT_SESSION_LOCK_V1
    if (m_lockScreen->isLocked()) {
        lock->finish();
        return;
    }

    m_lockScreen->onExternalLock(lock);

    setCurrentMode(CurrentMode::LockScreen);

    if (m_multitaskView) {
        m_multitaskView->immediatelyExit();
    }

    deleteTaskSwitch();
        
    setWorkspaceVisible(false);

    lock->safeConnect(&WSessionLock::abandoned, this, [this]() {
        m_lockScreenGraceTimer->stop();
        setNoAnimation(false);
    });

    lock->safeConnect(&WSessionLock::canceled, this, [this]() {
        m_lockScreenGraceTimer->stop();
    });

    m_lockScreenGraceTimer->disconnect();
    // grace 300ms for possible client to
    connect(m_lockScreenGraceTimer, &QTimer::timeout, this, [this, lock]() {
        setNoAnimation(true);
        lock->lock();
    });
    m_lockScreenGraceTimer->start();
#endif
}

void Helper::allowNonDrmOutputAutoChangeMode(WOutput *output)
{
    output->safeConnect(&qw_output::notify_request_state,
                        this,
                        [this](wlr_output_event_request_state *newState) {
                            if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
                                auto output = qobject_cast<qw_output *>(sender());

                                output->commit_state(newState->state);
                            }
                        });
}

int Helper::indexOfOutput(WOutput *output) const
{
    for (int i = 0; i < m_outputList.size(); i++) {
        if (m_outputList.at(i)->output() == output)
            return i;
    }
    return -1;
}

Output *Helper::getOutput(WOutput *output) const
{
    for (auto o : std::as_const(m_outputList)) {
        if (o->output() == output)
            return o;
    }
    return nullptr;
}

void Helper::addOutput()
{
    qobject_cast<qw_multi_backend *>(m_backend->handle())
        ->for_each_backend(
            [](wlr_backend *backend, void *) {
                if (auto x11 = qw_x11_backend::from(backend)) {
                    qw_output::from(x11->output_create());
                } else if (auto wayland = qw_wayland_backend::from(backend)) {
                    qw_output::from(wayland->output_create());
                }
            },
            nullptr);
}

void Helper::setOutputMode(OutputMode mode)
{
    if (m_outputList.length() < 2 || m_mode == mode)
        return;
    m_mode = mode;
    Q_EMIT outputModeChanged();
    for (int i = 0; i < m_outputList.size(); i++) {
        if (m_outputList.at(i) == m_rootSurfaceContainer->primaryOutput())
            continue;
        Output *o = nullptr;
        if (mode == OutputMode::Copy) {
            o = createCopyOutput(m_outputList.at(i)->output(),
                                 m_rootSurfaceContainer->primaryOutput());
            m_rootSurfaceContainer->removeOutput(m_outputList.at(i));
        } else if (mode == OutputMode::Extension) {
            o = createNormalOutput(m_outputList.at(i)->output());
            o->enable();
        }
        m_outputList.at(i)->deleteLater();
        m_outputList.replace(i, o);
    }
}

float Helper::animationSpeed() const
{
    return m_animationSpeed;
}

void Helper::setAnimationSpeed(float newAnimationSpeed)
{
    if (qFuzzyCompare(m_animationSpeed, newAnimationSpeed))
        return;
    m_animationSpeed = newAnimationSpeed;
    Q_EMIT animationSpeedChanged();
}

Helper::OutputMode Helper::outputMode() const
{
    return m_mode;
}

/**
 * Add a WSocket to the Wayland server. 
 * This function is used by Treeland::ActivateWayland.
 * 
 * @param socket WSocket to add
 */
void Helper::addSocket(WSocket *socket)
{
    m_server->addSocket(socket);
}

/**
 * Find the session for the given uid
 * 
 * @param uid User ID to find session for
 * @returns Session for the given uid, or nullptr if not found
 */
std::shared_ptr<Session> Helper::sessionForUid(uid_t uid) const
{
    for (auto session : m_sessions) {
        if (session && session->uid == uid)
            return session;
    }
    return nullptr;
}

/**
 * Find the session for the given WXWayland
 * 
 * @param xwayland WXWayland to find session for
 * @returns Session for the given xwayland, or nullptr if not found
 */
std::shared_ptr<Session> Helper::sessionForXWayland(WXWayland *xwayland) const
{
    for (auto session : m_sessions) {
        if (session && session->xwayland == xwayland)
            return session;
    }
    return nullptr;
}

/**
 * Find the session for the given WSocket
 * 
 * @param socket WSocket to find session for
 * @returns Session for the given socket, or nullptr if not found
 */
std::shared_ptr<Session> Helper::sessionForSocket(WSocket *socket) const
{
    for (auto session : m_sessions) {
        if (session && session->socket == socket)
            return session;
    }
    return nullptr;
}

/**
 * Get the currently active session
 *
 * @returns weak_ptr to the active session
 */
std::weak_ptr<Session> Helper::activeSession() const
{
    return m_activeSession;
}

/**
 * Remove a session from the session list
 *
 * @param session The session to remove
 */
void Helper::removeSession(std::shared_ptr<Session> session)
{
    if (!session)
        return;

    if (m_activeSession.lock() == session) {
        m_activeSession.reset();
        m_activatedSurface = nullptr;
        Q_EMIT activatedSurfaceChanged();
    }

    for (auto s : std::as_const(m_sessions)) {
        if (s.get() == session.get()) {
            m_sessions.removeOne(s);
            break;
        }
    }

    session.reset();
}

/**
 * Ensure a session exists for the given uid, creating it if necessary
 * 
 * @param id An existing logind session ID
 * @param uid User ID to ensure session for
 * @returns Session for the given uid, or nullptr on failure
 */
std::shared_ptr<Session> Helper::ensureSession(int id, uid_t uid)
{
    // Helper lambda to create WSocket and WXWayland
    auto createWSocket = [this]() {
        // Create WSocket
        auto socket = new WSocket(true, this);
        if (!socket->autoCreate()) {
            qCCritical(treelandCore) << "Failed to create Wayland socket";
            delete socket;
            return static_cast<WSocket *>(nullptr);
        }
        // Connect signals
        connect(socket, &WSocket::fullServerNameChanged, this, [this] {
            if (m_activeSession.lock())
                Q_EMIT socketFileChanged();
        });
        // Add socket to server
        m_server->addSocket(socket);
        return socket;
    };
    auto createXWayland = [this](WSocket *socket) {
        // Create xwayland
        auto *xwayland = m_shellHandler->createXWayland(m_server, m_seat, m_compositor, false);
        if (!xwayland) {
            qCCritical(treelandCore) << "Failed to create XWayland instance";
            return static_cast<WXWayland *>(nullptr);
        }
        // Bind xwayland to socket
        xwayland->setOwnsSocket(socket);
        // Connect signals
        connect(xwayland, &WXWayland::ready, this, [this, xwayland] {
            if (auto session = sessionForXWayland(xwayland)) {
                session->noTitlebarAtom =
                    internAtom(session->xwayland->xcbConnection(), _DEEPIN_NO_TITLEBAR, false);
                if (!session->noTitlebarAtom) {
                    qCWarning(treelandInput) << "Failed to intern atom:" << _DEEPIN_NO_TITLEBAR;
                }
                session->settingManager = new SettingManager(session->xwayland->xcbConnection(),
                                                             session->xwayland);
                session->settingManagerThread = new QThread(session->xwayland);

                session->settingManager->moveToThread(session->settingManagerThread);
                connect(session->settingManagerThread, &QThread::started, this, [this, session]{
                    const qreal scale = m_rootSurfaceContainer->window()->effectiveDevicePixelRatio();
                    QMetaObject::invokeMethod(session->settingManager, [session, scale]() {
                            session->settingManager->setGlobalScale(scale);
                            session->settingManager->apply();
                        }, Qt::QueuedConnection);

                    QObject::connect(Helper::instance()->window(),
                        &WOutputRenderWindow::effectiveDevicePixelRatioChanged,
                        session->settingManager,
                        [session](qreal dpr) {
                            session->settingManager->setGlobalScale(dpr);
                            session->settingManager->apply();
                        }, Qt::QueuedConnection);
                });

                connect(session->settingManagerThread, &QThread::finished, session->settingManagerThread, &QThread::deleteLater);
                session->settingManagerThread->start();
            }
        });
        return xwayland;
    };
    // Check if session already exists for uid
    if (auto session = sessionForUid(uid)) {
        // Ensure it has a socket and xwayland
        if (!session->socket) {
            auto *socket = createWSocket();
            if (!socket) {
                m_sessions.removeOne(session);
                return nullptr;
            }
            session->socket = socket;
        }
        if (!session->xwayland) {
            auto *xwayland = createXWayland(session->socket);
            if (!xwayland) {
                delete session->socket;
                m_sessions.removeOne(session);
                return nullptr;
            }

            session->xwayland = xwayland;
        }

        return session;
    }
    // Session does not exist, create new session with deleter
    auto session = std::make_shared<Session>();
    session->id = id;
    session->uid = uid;

    session->socket = createWSocket();
    if (!session->socket) {
        session.reset();
        return nullptr;
    }

    session->xwayland = createXWayland(session->socket);
    if (!session->xwayland) {
        session.reset();
        return nullptr;
    }

    // Add session to list
    m_sessions.append(session);
    return session;
}

/**
 * Get the WXWayland for the given uid
 * 
 * @param uid User ID to get WXWayland for
 * @returns WXWayland for the given uid, or nullptr if not found/created
 */
WXWayland *Helper::xwaylandForUid(uid_t uid)
{
    auto session = sessionForUid(uid);
    return session ? session->xwayland : nullptr;
}

/**
 * Get the WSocket for the given uid
 * 
 * @param uid User ID to get WSocket for
 * @returns WSocket for the given uid, or nullptr if not found/created
 */
WSocket *Helper::waylandSocketForUid(uid_t uid)
{
    auto session = sessionForUid(uid);
    return session ? session->socket : nullptr;
}

/** 
 * Get the WSocket for the active session
 * 
 * @returns WSocket for the active session, or nullptr if none active
 */
WSocket *Helper::defaultWaylandSocket() const
{
    auto ptr = m_activeSession.lock();
    if (ptr && ptr->socket)
        return ptr->socket;
    return nullptr;
}

/**
 * Get the WXWayland for the active session
 * 
 * @returns WXWayland for the active session, or nullptr if none active
 */
WXWayland *Helper::defaultXWaylandSocket() const
{
    auto ptr = m_activeSession.lock();
    if (ptr && ptr->xwayland)
        return ptr->xwayland;
    return nullptr;
}

/**
 * Update the active session to the given uid, creating it if necessary.
 * This will update XWayland visibility and emit socketFileChanged if the
 * active session changed.
 * 
 * @param username Username to set as active session
 */
void Helper::updateActiveUserSession(const QString &username, int id)
{
    // Get previous active session
    auto previous = m_activeSession.lock();
    // Get new session for uid, creating if necessary
    auto session = ensureSession(id, getpwnam(username.toLocal8Bit().data())->pw_uid);
    if (!session) {
        qCWarning(treelandInput) << "Failed to ensure session for user" << username;
        return;
    }
    if (previous != session) {
        // Update active session
        m_activeSession = session;
        // Clear activated surface
        setActivatedSurface(nullptr);
        Q_EMIT activatedSurfaceChanged();
        // Emit signal and update socket enabled state
        if (previous && previous->socket)
            previous->socket->setEnabled(false);
        session->socket->setEnabled(true);
        Q_EMIT socketFileChanged();
        // Notify session changed through DBus, treeland-sd will listen it to update envs
        Q_EMIT m_treeland->SessionChanged();
    }
    qCInfo(treelandCore) << "Listing on:" << session->socket->fullServerName();
}

/**
 * Check if the given WClient belongs to any XWayland session.
 * This is used in setFilter function for output managers.
 * 
 * @param client WClient to check
 * @returns true if the client is an XWayland client, false otherwise
 */
bool Helper::isXWaylandClient(WClient *client)
{
    for (auto session : m_sessions) {
        if (session && session->xwayland && session->xwayland->waylandClient() == client) {
            return true;
        }
    }
    return false;
}

PersonalizationV1 *Helper::personalization() const
{
    return m_personalization;
}

bool Helper::toggleDebugMenuBar()
{
    bool ok = false;

    const auto outputs = rootContainer()->outputs();
    if (outputs.isEmpty())
        return false;

    bool firstOutputDebugMenuBarIsVisible = false;
    if (auto menuBar = outputs.first()->debugMenuBar()) {
        firstOutputDebugMenuBarIsVisible = menuBar->isVisible();
    }

    for (const auto &output : outputs) {
        if (output->debugMenuBar()) {
            output->debugMenuBar()->setVisible(!firstOutputDebugMenuBarIsVisible);
            ok = true;
        }
    }

    return ok;
}

WindowManagementV1::DesktopState Helper::showDesktopState() const
{
    return m_showDesktop;
}

bool Helper::isLaunchpad(WLayerSurface *surface) const
{
    if (!surface) {
        return false;
    }

    auto scope = QString(surface->handle()->handle()->scope);

    return scope == "dde-shell/launchpad";
}

void Helper::handleWindowPicker(WindowPickerInterface *picker)
{
    connect(picker, &WindowPickerInterface::pick, this, [this, picker](const QString &hint) {
        auto windowPicker =
            qobject_cast<WindowPicker *>(qmlEngine()->createWindowPicker(m_rootSurfaceContainer));
        windowPicker->setHint(hint);
        connect(windowPicker,
                &WindowPicker::windowPicked,
                this,
                [this, picker, windowPicker](WSurfaceItem *surfaceItem) {
                    if (surfaceItem) {
                        auto credentials = WClient::getCredentials(
                            surfaceItem->surface()->waylandClient()->handle());
                        picker->sendWindowPid(credentials->pid);
                        windowPicker->deleteLater();
                    }
                });
        connect(picker,
                &WindowPickerInterface::beforeDestroy,
                windowPicker,
                &WindowPicker::deleteLater);
    });
}

RootSurfaceContainer *Helper::rootSurfaceContainer() const
{
    return m_rootSurfaceContainer;
}

void Helper::setMultitaskViewImpl(IMultitaskView *impl)
{
    m_multitaskView = impl;
}

void Helper::setLockScreenImpl(ILockScreen *impl)
{
#if !defined(DISABLE_DDM) || defined(EXT_SESSION_LOCK_V1)
    if (!impl) {
        if (m_lockScreen) {
            m_lockScreen = nullptr;
            delete m_lockScreen;
        }
        return;
    }

    m_lockScreen = new LockScreen(impl, m_rootSurfaceContainer);
    m_lockScreen->setZ(RootSurfaceContainer::LockScreenZOrder);
    m_lockScreen->setVisible(false);

    for (auto *output : m_rootSurfaceContainer->outputs()) {
        m_lockScreen->addOutput(output);
    }

    if (auto primaryOutput = m_rootSurfaceContainer->primaryOutput()) {
        m_lockScreen->setPrimaryOutputName(primaryOutput->output()->name());
    }

    connect(m_lockScreen, &LockScreen::unlock, this, [this] {
        setCurrentMode(CurrentMode::Normal);
        setWorkspaceVisible(true);
#ifdef EXT_SESSION_LOCK_V1
        setNoAnimation(false);
#endif
        if (m_activatedSurface) {
            m_activatedSurface->setFocus(true, Qt::NoFocusReason);
        }
    });
    if (!impl) {
        return;
    }
    if (CmdLine::ref().useLockScreen()) {
        showLockScreen(false);
    }
#else
    Q_UNUSED(impl)
#endif
}

void Helper::setCurrentMode(CurrentMode mode)
{
    if (m_currentMode == mode)
        return;

    setBlockActivateSurface(mode != CurrentMode::Normal);

    m_currentMode = mode;

    Q_EMIT currentModeChanged();
}

void Helper::showLockScreen(bool switchToGreeter)
{
    if (m_lockScreen->isLocked()) {
        return;
    }

    setCurrentMode(CurrentMode::LockScreen);
    m_lockScreen->lock();

    setWorkspaceVisible(false);

    if (m_multitaskView) {
        m_multitaskView->immediatelyExit();
    }

    deleteTaskSwitch();

    // send DDM switch to greeter mode
    // FIXME: DDM and Treeland should listen to the lock signal of login1
    if (switchToGreeter) {
        QThreadPool::globalInstance()->start([]() {
            QDBusInterface interface("org.freedesktop.DisplayManager",
                                     "/org/freedesktop/DisplayManager/Seat0",
                                     "org.freedesktop.DisplayManager.Seat",
                                     QDBusConnection::systemBus());
            interface.call("SwitchToGreeter");
        });
    }
}

WSeat *Helper::seat() const
{
    return m_seat;
}

void Helper::handleLeftButtonStateChanged(const QInputEvent *event)
{
    Q_ASSERT(m_ddeShellV1 && m_seat);
    const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            DDEActiveInterface::sendActiveIn(DDEActiveInterface::Mouse, m_seat);
        } else {
            DDEActiveInterface::sendActiveOut(DDEActiveInterface::Mouse, m_seat);
        }
    }
}

void Helper::handleWhellValueChanged(const QInputEvent *event)
{
    Q_ASSERT(m_ddeShellV1 && m_seat);
    const QWheelEvent *we = static_cast<const QWheelEvent *>(event);
    QPoint delta = we->angleDelta();
    if (delta.x() + delta.y() < 0) {
        DDEActiveInterface::sendActiveOut(DDEActiveInterface::Wheel, m_seat);
    }
    if (delta.x() + delta.y() > 0) {
        DDEActiveInterface::sendActiveIn(DDEActiveInterface::Wheel, m_seat);
    }
}

void Helper::restoreFromShowDesktop(SurfaceWrapper *activeSurface)
{
    if (m_showDesktop == WindowManagementV1::DesktopState::Show) {
        m_showDesktop = WindowManagementV1::DesktopState::Normal;
        m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Normal);
        if (activeSurface) {
            activeSurface->requestCancelMinimize();
        }
        const auto &surfaces = getWorkspaceSurfaces();
        for (auto &surface : surfaces) {
            if (!surface->isMinimized() && !surface->isVisible()) {
                surface->setHideByShowDesk(true);
                surface->setSurfaceState(SurfaceWrapper::State::Minimized);
            }
        }
    }
}

Output *Helper::getOutputAtCursor() const
{
    QPoint cursorPos = QCursor::pos();
    for (auto output : m_outputList) {
        QRectF outputGeometry(output->outputItem()->position(), output->outputItem()->size());
        if (outputGeometry.contains(cursorPos)) {
            return output;
        }
    }

    return m_rootSurfaceContainer->primaryOutput();
}

void Helper::handleNewForeignToplevelCaptureRequest(wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request)
{
    if (!request || !request->toplevel_handle) {
        qCWarning(treelandCapture) << "Invalid capture request or toplevel handle";
        return;
    }

    auto *qw_handle = qw_ext_foreign_toplevel_handle_v1::from(request->toplevel_handle);
    WToplevelSurface *toplevelSurface = m_extForeignToplevelListV1->findSurfaceByHandle(qw_handle);
    if (!toplevelSurface) {
        qCWarning(treelandCapture) << "Could not find toplevel surface for handle";
        return;
    }

    SurfaceWrapper *surfaceWrapper = m_rootSurfaceContainer->getSurface(toplevelSurface);
    if (!surfaceWrapper) {
        qCWarning(treelandCapture) << "Could not find SurfaceWrapper for toplevel surface";
        return;
    }

    WSurfaceItem *surfaceItem = surfaceWrapper->surfaceItem();
    if (!surfaceItem) {
        qCWarning(treelandCapture) << "Could not get WSurfaceItem from SurfaceWrapper";
        return;
    }

    WSurfaceItemContent *surfaceContent = surfaceItem->findItemContent();
    if (!surfaceContent) {
        qCWarning(treelandCapture) << "Could not find WSurfaceItemContent";
        return;
    }

    qCDebug(treelandCapture) << "Found WSurfaceItemContent for capture:"
             << "size=" << surfaceContent->size()
             << "implicitSize=" << QSizeF(surfaceContent->implicitWidth(), surfaceContent->implicitHeight())
             << "isTextureProvider=" << surfaceContent->isTextureProvider();

    auto *output = surfaceWrapper->ownsOutput()->output();
    if (!output) {
        qCWarning(treelandCapture) << "Could not get WOutput from SurfaceWrapper";
        return;
    }

    auto *imageCaptureSource = new WExtImageCaptureSourceV1Impl(surfaceContent, output);

    bool success = qw_ext_foreign_toplevel_image_capture_source_manager_v1::request_accept(
        request, *imageCaptureSource);

    if (!success) {
        qCWarning(treelandCapture) << "Failed to accept foreign toplevel image capture request";
        delete imageCaptureSource;
    }
}

void Helper::onPrepareForSleep(bool sleep)
{
    if (sleep) {
        qCInfo(treelandCore) << "Rendering black frames to all outputs before hibernate";
        disableRender();
        // TODO：should we disable output？
    } else {
        qCInfo(treelandCore) << "Re-enabled rendering after hibernate";
        enableRender();
    }
}

UserModel *Helper::userModel() const {
    return m_userModel;
}

DDMInterfaceV1 *Helper::ddmInterfaceV1() const {
    return m_ddmInterfaceV1;
}

void Helper::activateSession() {
    if (!m_backend->isSessionActive())
        m_backend->activateSession();
}

void Helper::deactivateSession() {
    if (m_backend->isSessionActive())
        m_backend->deactivateSession();
}

void Helper::enableRender() {
    m_renderWindow->setRenderEnabled(true);
}

void Helper::disableRender() {
    m_renderWindow->setRenderEnabled(false);

    // Revoke all evdev devices to prevent accidental events during switch
    static const char prefix[] = "/dev/input/";
    static const int prefixLen = strlen(prefix);
    struct wlr_session *session = m_backend->session()->handle();
    struct wlr_device *device = nullptr;
    wl_list_for_each(device, &session->devices, link) {
        char path[32];
        if (readlink(qPrintable(QStringLiteral("/proc/self/fd/%1").arg(device->fd)), path, 32) < 0)
            qCWarning(treelandCore) << "Failed to read path of file descriptor " << device->fd;
        else if (strncmp(prefix, path, prefixLen))
            ioctl(device->fd, EVIOCREVOKE, nullptr);
    }
}

void Helper::setBlockActivateSurface(bool block)
{
    if (block == m_blockActivateSurface)
        return;
    m_blockActivateSurface = block;
    Q_EMIT blockActivateSurfaceChanged();
}

bool Helper::blockActivateSurface() const
{
    return m_blockActivateSurface;
}

bool Helper::noAnimation() const {
    return m_noAnimation;
}

void Helper::setNoAnimation(bool noAnimation) {
    if (m_noAnimation == noAnimation)
        return;
    m_noAnimation = noAnimation;
    emit noAnimationChanged();
}
void Helper::toggleFpsDisplay()
{
    if (m_fpsDisplay) {
        m_fpsDisplay->deleteLater();
        m_fpsDisplay = nullptr;
        return;
    }

    m_fpsDisplay = qmlEngine()->createFpsDisplay(m_renderWindow->contentItem());
}

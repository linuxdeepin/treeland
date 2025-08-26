// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include "modules/capture/capture.h"
#include "utils/cmdline.h"
#include "modules/dde-shell/ddeshellattached.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "input/inputdevice.h"
#include "core/layersurfacecontainer.h"
#include "greeter/usermodel.h"
#include "utils/fpsdisplaymanager.h"

#include <rhi/qrhi.h>

#include <QDBusConnection>
#include <QDBusInterface>
#ifndef DISABLE_DDM
#  include "core/lockscreen.h"
#endif
#include "interfaces/multitaskviewinterface.h"
#include "output/output.h"
#include "modules/primary-output/outputmanagement.h"
#include "modules/personalization/personalizationmanager.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "modules/shortcut/shortcutmanager.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "input/togglablegesture.h"
#include "config/treelandconfig.h"
#include "modules/wallpaper-color/wallpapercolor.h"
#include "core/windowpicker.h"
#include "workspace/workspace.h"

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

#include <qwallocator.h>
#include <qwbackend.h>
#include <qwbuffer.h>
#include <qwcompositor.h>
#include <qwdatacontrolv1.h>
#include <qwdatadevice.h>
#include <qwdisplay.h>
#include <qwfractionalscalemanagerv1.h>
#include <qwgammacontorlv1.h>
#include <qwlayershellv1.h>
#include <qwlogging.h>
#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwscreencopyv1.h>
#include <qwsubcompositor.h>
#include <qwviewporter.h>
#include <qwxwaylandsurface.h>
#include <qwoutputpowermanagementv1.h>
#include <qwidlenotifyv1.h>
#include <qwidleinhibitv1.h>

#include <QAction>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtConcurrent>

#include <pwd.h>
#include <utility>

#define WLR_FRACTIONAL_SCALE_V1_VERSION 1
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

Q_LOGGING_CATEGORY(qLcHelper, "treeland.helper");

Helper *Helper::m_instance = nullptr;
static bool s_settingSeatPosition = false;

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
    , m_renderWindow(new WOutputRenderWindow(this))
    , m_server(new WServer(this))
    , m_rootSurfaceContainer(new RootSurfaceContainer(m_renderWindow->contentItem()))
    , m_windowGesture(new TogglableGesture(this))
    , m_multiTaskViewGesture(new TogglableGesture(this))
{
    Q_ASSERT(!m_instance);
    m_instance = this;

    m_renderWindow->setColor(Qt::black);
    m_rootSurfaceContainer->setFlag(QQuickItem::ItemIsFocusScope, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    m_rootSurfaceContainer->setFocusPolicy(Qt::StrongFocus);
#endif

    m_shellHandler = new ShellHandler(m_rootSurfaceContainer);

    m_workspaceScaleAnimation = new QPropertyAnimation(m_shellHandler->workspace(), "scale", this);
    m_workspaceOpacityAnimation =
        new QPropertyAnimation(m_shellHandler->workspace(), "opacity", this);

    m_workspaceScaleAnimation->setDuration(1000);
    m_workspaceOpacityAnimation->setDuration(1000);
    m_workspaceScaleAnimation->setEasingCurve(QEasingCurve::OutExpo);
    m_workspaceOpacityAnimation->setEasingCurve(QEasingCurve::OutExpo);

    connect(m_renderWindow, &QQuickWindow::activeFocusItemChanged, this, [this]() {
        auto wrapper = keyboardFocusSurface();
        if (!wrapper)
            return;

        WSeat *interactingSeat = m_currentEventSeat ? m_currentEventSeat : getLastInteractingSeat(wrapper);

        if (interactingSeat) {
            updateSurfaceSeatInteraction(wrapper, interactingSeat);
            interactingSeat->setKeyboardFocusSurface(wrapper->surface());
            m_seatKeyboardFocusSurfaces[interactingSeat] = wrapper;
        } else {
            for (auto seat : m_seatManager->seats()) {
                if (seat->pointerFocusSurface() == wrapper->surface()) {
                    seat->setKeyboardFocusSurface(wrapper->surface());
                    m_seatKeyboardFocusSurfaces[seat] = wrapper;
                    updateSurfaceSeatInteraction(wrapper, seat);
                    break;
                }
            }
        }
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

    connect(&TreelandConfig::ref(),
            &TreelandConfig::cursorThemeNameChanged,
            this,
            &Helper::cursorThemeChanged);
    connect(&TreelandConfig::ref(),
            &TreelandConfig::cursorSizeChanged,
            this,
            &Helper::cursorSizeChanged);
}

Helper::~Helper()
{
    if (m_renderWindow) {
        m_renderWindow->disconnect();
    }

    if (m_backend) {
        m_backend->disconnect();
    }

    if (m_rootSurfaceContainer) {
        for (auto s : m_rootSurfaceContainer->surfaces()) {
            if (auto c = s->container())
                c->removeSurface(s);
        }
        delete m_rootSurfaceContainer;
        m_rootSurfaceContainer = nullptr;
    }

    m_seatActivatedSurfaces.clear();
    m_seatMoveResizeStates.clear();
    m_seatLastPressedPositions.clear();
    m_seatMetaKeyStates.clear();
    m_seatKeyboardFocusSurfaces.clear();

    m_currentEventSeat = nullptr;

    Q_ASSERT(m_instance == this);
    m_instance = nullptr;
}

Helper *Helper::instance()
{
    return m_instance;
}

bool Helper::getSeatMoveResizeInfo(SurfaceWrapper *surface, Qt::Edges &edges, QRectF &startGeometry)
{
    if (!m_instance)
        return false;

    for (auto it = m_instance->m_seatMoveResizeStates.begin();
         it != m_instance->m_seatMoveResizeStates.end(); ++it) {
        if (it.value().surface == surface) {
            edges = it.value().edges;
            startGeometry = it.value().startGeometry;
            return true;
        }
    }
    return false;
}

bool Helper::isSettingSeatPosition()
{
    return s_settingSeatPosition;
}

void Helper::setSeatPositionFlag(bool setting)
{
    s_settingSeatPosition = setting;
}

bool Helper::isNvidiaCardPresent()
{
    auto rhi = m_renderWindow->rhi();

    if (!rhi)
        return false;

    QString deviceName = rhi->driverInfo().deviceName;
    qCDebug(qLcHelper) << "Graphics Device:" << deviceName;

    return deviceName.contains("NVIDIA", Qt::CaseInsensitive);
}

void Helper::setWorkspaceVisible(bool visible)
{
    for (auto *surface : m_rootSurfaceContainer->surfaces()) {
        if (surface->type() == SurfaceWrapper::Type::Layer) {
            surface->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
        }
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
    Output *o;
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
        qCWarning(qLcHelper) << "Failed to set gamma lut!";
        // TODO: use software impl it.
        qw_gamma_control_v1::from(gamma_control)->send_failed_and_destroy();
    }
}

void Helper::onOutputTestOrApply(qw_output_configuration_v1 *config, bool onlyTest)
{
    QList<WOutputState> states = m_outputManager->stateListPending();
    bool ok = true;
    for (auto state : std::as_const(states)) {
        WOutput *output = state.output;
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
            if (!onlyTest) {
                newState.set_transform(static_cast<wl_output_transform>(state.transform));
                newState.set_scale(state.scale);

                WOutputViewport *viewport = getOutput(output)->screenViewport();
                if (viewport) {
                    auto outputItem = qobject_cast<WOutputItem*>(viewport->parentItem());
                    if (outputItem) {
                        outputItem->setX(state.x);
                        outputItem->setY(state.y);
                    }
                }
            }
        }

        if (onlyTest)
            ok &= output->handle()->test_state(newState);
        else
            ok &= output->handle()->commit_state(newState);
    }
    if (ok && !onlyTest) {
        QString cache_location = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QSettings settings(cache_location + "/output.ini", QSettings::IniFormat);
        for (WOutputState state : std::as_const(states)) {
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
    for (int i = 0; i < m_outputList.size(); i++) {
        Output *currentOutput = m_outputList.at(i);
        if (currentOutput->output()->name() == virtual_output->outputList.at(0))
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

    if (isXdgToplevel || isXdgPopup || isLayer) {
        auto *attached =
            new Personalization(wrapper->shellSurface(), m_personalization, wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                attached, &Personalization::deleteLater);

        // Ensure this wrapper is removed from active history when it begins invalidation
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated, this, [this, wrapper] {
            if (auto ws = workspace())
                ws->removeActivedSurface(wrapper);
        });

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
        auto xwayland = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
        auto updateDecorationTitleBar = [this, wrapper, xwayland]() {
            if (!xwayland->isBypassManager()) {
                if (m_atomDeepinNoTitlebar
                    && !readWindowProperty(defaultXWaylandSocket()->xcbConnection(),
                                           xwayland->handle()->handle()->window_id,
                                           m_atomDeepinNoTitlebar,
                                           XCB_ATOM_CARDINAL)
                            .isEmpty()) {
                    wrapper->setNoTitleBar(true);
                } else {
                    wrapper->setNoTitleBar(xwayland->decorationsFlags()
                                           & WXWaylandSurface::DecorationsNoTitle);
                }
                wrapper->setNoDecoration(xwayland->decorationsFlags()
                                         & WXWaylandSurface::DecorationsNoBorder);
            } else {
                wrapper->setNoTitleBar(true);
                wrapper->setNoDecoration(true);
            }
        };
        // When x11 surface dissociate, SurfaceWrapper will be destroyed immediately
        // but WXWaylandSurface will not, so must connect to `wrapper`
        xwayland->safeConnect(&WXWaylandSurface::bypassManagerChanged,
                              wrapper,
                              updateDecorationTitleBar);
        xwayland->safeConnect(&WXWaylandSurface::decorationsFlagsChanged,
                              wrapper,
                              updateDecorationTitleBar);
        updateDecorationTitleBar();
    }

    if (!isLayer) {
        auto windowOverlapChecker = new WindowOverlapChecker(wrapper, wrapper);
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
        m_treelandForeignToplevel->addSurface(wrapper);
    }
    connect(wrapper, &SurfaceWrapper::skipDockPreViewChanged, this, [this, wrapper] {
        if (wrapper->skipDockPreView()) {
            m_foreignToplevel->removeSurface(wrapper->shellSurface());
            m_treelandForeignToplevel->removeSurface(wrapper);
        } else {
            m_foreignToplevel->addSurface(wrapper->shellSurface());
            m_treelandForeignToplevel->addSurface(wrapper);
        }
    });

    connect(wrapper,
            &SurfaceWrapper::destroyed,
            this,
            [this](QObject *obj) {
                auto wrapper = static_cast<SurfaceWrapper *>(obj);

                for (auto seat : m_seatManager->seats()) {
                    if (m_seatActivatedSurfaces.value(seat) == wrapper) {
                        m_seatActivatedSurfaces.remove(seat);
                    }
                    if (m_seatMoveResizeStates.value(seat).surface == wrapper) {
                        endMoveResizeForSeat(seat);
                    }
                }

                if (m_activatedSurface == wrapper) {
                    m_activatedSurface = nullptr;
                    Q_EMIT activatedSurfaceChanged();
                }
            });

    if (isXdgToplevel || isXwayland) {
        connect(wrapper, &SurfaceWrapper::requestMinimize, this, [this, wrapper]() {
            if (TreelandConfig::ref().blockActivateSurface())
                return;

            if (m_currentMode == CurrentMode::Normal) {
                if (wrapper->surfaceState() == SurfaceWrapper::State::Minimized)
                    return;

                auto container = wrapper->container();
                if (container) {
                    container->removeSurface(wrapper);
                }
            }
        });
    }

    if (isXdgToplevel || isXwayland) {
        connect(wrapper, &SurfaceWrapper::requestMove, this, [this, wrapper]() {
            WSeat *requestingSeat = nullptr;

            if (m_currentEventSeat) {
                requestingSeat = m_currentEventSeat;
            }

            if (!requestingSeat) {
                requestingSeat = findSeatForSurface(wrapper);
            }

            if (!requestingSeat && !m_seatManager->seats().isEmpty()) {
                requestingSeat = m_seatManager->seats().first();
            }

            if (!requestingSeat) {
                return;
            }

            beginMoveResizeForSeat(requestingSeat, wrapper, Qt::Edges{});
        }, Qt::DirectConnection);

        connect(wrapper, &SurfaceWrapper::requestResize, this, [this, wrapper](Qt::Edges edges) {
            WSeat *requestingSeat = nullptr;

            if (m_currentEventSeat) {
                requestingSeat = m_currentEventSeat;
            }

            if (!requestingSeat) {
                requestingSeat = findSeatForSurface(wrapper);
            }

            if (!requestingSeat && !m_seatManager->seats().isEmpty()) {
                requestingSeat = m_seatManager->seats().first();
            }

            if (!requestingSeat) {
                return;
            }

            beginMoveResizeForSeat(requestingSeat, wrapper, edges);
        }, Qt::DirectConnection);
    }
}

void Helper::onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper)
{
    if (!wrapper->skipDockPreView()) {
        m_foreignToplevel->removeSurface(wrapper->shellSurface());
        m_treelandForeignToplevel->removeSurface(wrapper);
    }
    // Ensure the wrapper is removed from active history early to avoid cascading on half-invalid entries
    if (wrapper) {
        auto ws = workspace();
        if (ws)
            ws->removeActivedSurface(wrapper);
    }
}

bool Helper::surfaceBelongsToCurrentUser(SurfaceWrapper *wrapper)
{
    static const int puid = getuid();
    auto credentials = WClient::getCredentials(wrapper->surface()->waylandClient()->handle());
    auto user = m_userModel->currentUser();
    if (user) {
        // FIXME: XWayland's surfaces' uid are all puid now, will change to per user
        // XWayland instance in the future
        return credentials->uid == user->UID() || credentials->uid == puid;
    } else {
        return credentials->uid == puid;
    }
}

void Helper::deleteTaskSwitch()
{
    if (m_taskSwitch) {
        m_taskSwitch->deleteLater();
        m_taskSwitch = nullptr;
    }
}

void Helper::init()
{
    auto engine = qmlEngine();
    m_userModel = engine->singletonInstance<UserModel *>("Treeland", "UserModel");

    m_fpsDisplayManager = new FpsDisplayManager(this);
    m_fpsDisplayManager->setTargetWindow(m_renderWindow);

    engine->setContextForObject(m_renderWindow, engine->rootContext());
    engine->setContextForObject(m_renderWindow->contentItem(), engine->rootContext());
    m_rootSurfaceContainer->setQmlEngine(engine);
    m_rootSurfaceContainer->init(m_server);

    m_backend = m_server->attach<WBackend>();
    m_seatManager = m_server->attach<WSeatManager>();

    m_outputManager = m_server->attach<WOutputManagerV1>();
    connect(m_backend, &WBackend::outputAdded, this, &Helper::onOutputAdded);
    connect(m_backend, &WBackend::outputRemoved, this, &Helper::onOutputRemoved);

    connect(m_backend, &WBackend::inputRemoved, this, [this](WInputDevice *device) {
        for (auto seat : m_seatManager->seats()) {
            if (seat->deviceList().contains(device)) {
                seat->detachInputDevice(device);
            }
        }
    });

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

    m_server->start();
    initMultiSeat();
    m_shellHandler->initInputMethodHelper(m_server, m_seat);

    m_foreignToplevel = m_server->attach<WForeignToplevel>();
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

    m_primaryOutputV1 = m_server->attach<PrimaryOutputV1>();
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

    connect(m_primaryOutputV1,
            &PrimaryOutputV1::requestSetPrimaryOutput,
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
            m_primaryOutputV1->sendPrimaryOutput(
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

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());
    if (!m_renderer) {
        qCFatal(qLcHelper) << "Failed to create renderer";
    }

    m_allocator = qw_allocator::autocreate(*m_backend->handle(), *m_renderer);
    m_renderer->init_wl_display(*m_server->handle());

    // free follow display
    m_compositor = qw_compositor::create(*m_server->handle(), 6, *m_renderer);
    qw_subcompositor::create(*m_server->handle());
    qw_screencopy_manager_v1::create(*m_server->handle());
    qw_viewporter::create(*m_server->handle());
    m_renderWindow->init(m_renderer, m_allocator);

    // for xwayland
    auto *xwaylandOutputManager =
        m_server->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());
    xwaylandOutputManager->setScaleOverride(1.0);
    m_defaultXWayland = m_shellHandler->createXWayland(m_server, m_seat, m_compositor, false);
    connect(m_defaultXWayland, &WXWayland::ready, this, [this] {
        m_atomDeepinNoTitlebar =
            internAtom(m_defaultXWayland->xcbConnection(), _DEEPIN_NO_TITLEBAR, false);
        if (!m_atomDeepinNoTitlebar) {
            qWarning() << "failed internAtom:" << _DEEPIN_NO_TITLEBAR;
        }
    });
    xdgOutputManager->setFilter([this] (WClient *client) {
        return client != m_defaultXWayland->waylandClient();
    });
    xwaylandOutputManager->setFilter([this] (WClient *client) {
        return client == m_defaultXWayland->waylandClient();
    });
    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();
    connect(m_xdgDecorationManager,
            &WXdgDecorationManager::surfaceModeChanged,
            this,
            &Helper::onSurfaceModeChanged);

    bool freezeClientWhenDisable = false;
    m_socket = new WSocket(freezeClientWhenDisable);
    if (m_socket->autoCreate()) {
        m_server->addSocket(m_socket);
        Q_EMIT socketFileChanged();
    } else {
        delete m_socket;
        qCCritical(qLcHelper) << "Failed to create socket";
        return;
    }

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

    m_outputPowerManager = qw_output_power_manager_v1::create(*m_server->handle());

    connect(m_outputPowerManager, &qw_output_power_manager_v1::notify_set_mode, this, &Helper::onSetOutputPowerMode);

    m_backend->handle()->start();

    qCInfo(qLcHelper) << "Listing on:" << m_socket->fullServerName();
}

void Helper::initMultiSeat()
{
    m_seatConfigPath = "/etc/deepin/treeland/seats.json";
    loadSeatConfig();

    if (m_seatManager->seats().isEmpty()) {
        WSeat *defaultSeat = m_seatManager->createSeat("seat0", true);
        if (!defaultSeat) {
            qCritical() << "Failed to create default seat!";
            return;
        }

        if (!defaultSeat->nativeHandle()) {
            m_server->attach(defaultSeat);
        }

        m_seat = defaultSeat;
        m_seatManager->addDeviceRule("seat0", "1:.*"); // Keyboard devices
        m_seatManager->addDeviceRule("seat0", "2:.*"); // Pointer devices
        m_seatManager->addDeviceRule("seat0", "3:.*"); // Touch devices
    } else {
        m_seat = m_seatManager->seats().first();
        if (!m_seat->nativeHandle()) {
            m_server->attach(m_seat);
        }
        if (m_seatManager->deviceRules(m_seat->name()).isEmpty()) {
            m_seatManager->addDeviceRule(m_seat->name(), "1:.*");
            m_seatManager->addDeviceRule(m_seat->name(), "2:.*");
            m_seatManager->addDeviceRule(m_seat->name(), "3:.*");
        }
    }

    if (!m_seat) {
        m_seat = m_seatManager->createSeat("seat0", true);
        if (!m_seat) {
            qCritical() << "Critical error: Cannot create any seat!";
            return;
        }

        if (!m_seat->nativeHandle()) {
            m_server->attach(m_seat);
        }
    }

    if (!m_seat->eventFilter()) {
        m_seat->setEventFilter(this);
    }

    setupSeatsConfiguration();
    connectDeviceSignals();
    assignExistingDevices();

    QTimer::singleShot(1000, this, &Helper::checkAndFixSeatDevices);
}

void Helper::loadSeatConfig()
{
    QFile file(m_seatConfigPath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject config = doc.object();

        if (validateSeatConfig(config)) {
            m_seatManager->loadConfig(config);
        }
    }
}

void Helper::saveSeatConfig()
{
    QFile file(m_seatConfigPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(m_seatManager->saveConfig());
        file.write(doc.toJson());
    }
}

WSeatManager *Helper::seatManager() const
{
    return m_seatManager;
}

bool Helper::validateSeatConfig(const QJsonObject &config) const
{
    if (!config.contains("seats") || !config["seats"].isArray()) {
        return false;
    }

    QJsonArray seatsArray = config["seats"].toArray();
    bool hasFallback = false;

    for (const auto &seatValue : seatsArray) {
        if (!seatValue.isObject()) {
            return false;
        }

        QJsonObject seatObj = seatValue.toObject();
        if (!seatObj.contains("name") || !seatObj["name"].isString()) {
            return false;
        }

        if (seatObj.contains("fallback") && seatObj["fallback"].toBool()) {
            if (hasFallback) {
                qWarning() << "Multiple fallback seats found in configuration";
                return false;
            }
            hasFallback = true;
        }

        if (seatObj.contains("deviceRules") && !seatObj["deviceRules"].isArray()) {
            return false;
        }
    }

    return true;
}

WSeat *Helper::getSeatForDevice(WInputDevice *device) const
{
    if (!device) {
        return nullptr;
    }

    for (auto seat : m_seatManager->seats()) {
        if (seat->deviceList().contains(device)) {
            return seat;
        }
    }

    QString deviceName = device->name();
    WInputDevice::Type deviceType = device->type();
    QString deviceInfo = QString("%1:%2").arg(static_cast<int>(deviceType)).arg(deviceName);

    for (auto seat : m_seatManager->seats()) {
        QStringList rules = m_seatManager->deviceRules(seat->name());
        for (const QString &ruleStr : rules) {
            QRegularExpression rule(ruleStr);
            if (rule.isValid() && rule.match(deviceInfo).hasMatch()) {
                return seat;
            }
        }
    }

    return m_seatManager->fallbackSeat();
}

bool Helper::socketEnabled() const
{
    return m_socket->isEnabled();
}

void Helper::setSocketEnabled(bool newEnabled)
{
    if (m_socket)
        m_socket->setEnabled(newEnabled);
    else
        qCWarning(qLcHelper) << "Can't set enabled for empty socket!";
}

void Helper::activateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason)
{
    WSeat *interactingSeat = m_currentEventSeat ? m_currentEventSeat : getLastInteractingSeat(wrapper);

    if (TreelandConfig::ref().blockActivateSurface() && wrapper) {
        auto sh = wrapper->shellSurface();
        if (sh && wrapper->surface() && wrapper->surface()->mapped()
            && sh->hasCapability(WToplevelSurface::Capability::Activate)) {
            workspace()->pushActivedSurface(wrapper);
        }
        return;
    }

    if (!wrapper) {
        setActivatedSurface(nullptr);
    } else {
        auto sh = wrapper->shellSurface();
        if (!sh || !sh->hasCapability(WToplevelSurface::Capability::Activate)) {
            // do nothing; see comment below about dock behavior
        } else {
            if (wrapper->hasActiveCapability()) {
                setActivatedSurface(wrapper);
            } else {
                qCritical() << "Try activate a surface which don't have ActiveCapability!";
            }
        }
    }

    if (!wrapper
        || (wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus)
            && wrapper->acceptKeyboardFocus())) {

        WSeat *targetSeat = interactingSeat ? interactingSeat : m_seat;

        if (targetSeat && targetSeat->nativeHandle()) {
            if (wrapper) {
                wrapper->setFocus(true, reason);
            } else if (auto currentFocus = keyboardFocusSurface()) {
                currentFocus->setFocus(false, reason);
            }

            if (wrapper) {
                if (m_currentEventSeat && targetSeat == m_currentEventSeat) {
                    updateSurfaceSeatInteraction(wrapper, targetSeat);
                }
            }

            targetSeat->setKeyboardFocusSurface(wrapper ? wrapper->surface() : nullptr);
            if (wrapper) {
                m_seatKeyboardFocusSurfaces[targetSeat] = wrapper;
            } else {
                m_seatKeyboardFocusSurfaces.remove(targetSeat);
            }
        }
    }
}

void Helper::forceActivateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason)
{
    if (!wrapper) {
        qCCritical(qLcHelper) << "Don't force activate to empty surface! do you want `Helper::activeSurface(nullptr)`?";
        return;
    }

    restoreFromShowDesktop(wrapper);

    if (wrapper->isMinimized()) {
        wrapper->requestCancelMinimize(
            !(reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason));
    }

    if (!wrapper->surface()->mapped()) {
        qCWarning(qLcHelper) << "Can't activate unmapped surface: " << wrapper;
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

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *targetWindow, QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend) {
        return false;
    }

    if (Q_UNLIKELY(!targetWindow || !event)) {
        return false;
    }

    if (event->isInputEvent()) {
        m_idleNotifier->notify_activity(seat->nativeHandle());
    }

    WSeat *eventSeat = getSeatForEvent(event);
    m_currentEventSeat = eventSeat;
    WSeat *targetSeat = seat;
    if (event->device()) {
        WInputDevice *device = WInputDevice::from(event->device());
        if (device) {
            targetSeat = getSeatForDevice(device);
            if (!targetSeat) {
                qWarning() << "Device has no associated seat, using default seat";
                targetSeat = seat;
            }
        }
    }

    if (targetSeat && targetSeat != seat) {
        return false;
    }

    if (seat == m_seat) {
        // NOTE: Unable to distinguish meta from other key combinations
        //       For example, Meta+S will still receive Meta release after
        //       fully releasing the key, actively detect whether there are
        //       other keys, and reset the state.
        if (event->type() == QEvent::KeyPress) {
            auto kevent = static_cast<QKeyEvent *>(event);

            switch (kevent->key()) {
            case Qt::Key_Meta:
            case Qt::Key_Super_L:
                setSingleMetaKeyPendingPressed(seat, true);
                break;
            default:
                setSingleMetaKeyPendingPressed(seat, false);
                break;
            }

            if (QKeySequence(kevent->keyCombination())
                == QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier | Qt::Key_F11)) {
                if (toggleDebugMenuBar())
                    return true;
            }

            if (m_currentMode == CurrentMode::Normal
                && QKeySequence(kevent->modifiers() | kevent->key())
                    == QKeySequence(Qt::ControlModifier | Qt::AltModifier | Qt::Key_Delete)) {
                setCurrentMode(CurrentMode::LockScreen);
                m_lockScreen->shutdown();
                setWorkspaceVisible(false);
                return true;
            }

            if (QKeySequence(kevent->modifiers() | kevent->key())
                == QKeySequence(Qt::META | Qt::Key_F12)) {
                qApp->quit();
                return true;
            }
            else if (QKeySequence(kevent->modifiers() | kevent->key())
                == QKeySequence(Qt::META | Qt::Key_F11)) {
                toggleFpsDisplay();
                return true;
            }
            else if (m_captureSelector) {
                if (event->modifiers() == Qt::NoModifier && kevent->key() == Qt::Key_Escape)
                    m_captureSelector->cancelSelection();
            }
            else if (event->modifiers() == Qt::MetaModifier) {
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
                    int workspaceIndex = switchWorkspaceNums.indexOf(kevent->key());
                    switchWorkspaceForSeat(seat, workspaceIndex);
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
                } else if (kevent->key() == Qt::Key_L) {
                    if (m_lockScreen->isLocked()) {
                        return true;
                    }

                    showLockScreen();
                    return true;
#endif
                } else if (kevent->key() == Qt::Key_D) {
                    if (m_currentMode == CurrentMode::Multitaskview) {
                        return true;
                    }
                    if (m_showDesktop == WindowManagementV1::DesktopState::Normal)
                        m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Show);
                    else if (m_showDesktop == WindowManagementV1::DesktopState::Show)
                        m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Normal);
                    return true;
                } else if (kevent->key() == Qt::Key_Up && m_activatedSurface) {
                    m_activatedSurface->requestMaximize();
                    return true;
                } else if (kevent->key() == Qt::Key_Down && m_activatedSurface) {
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
                if (kevent->key() == Qt::Key_F4 && m_activatedSurface) {
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
        } else if (event->type() == QEvent::KeyRelease) {
            auto kevent = static_cast<QKeyEvent *>(event);
            if ((kevent->key() == Qt::Key_Meta || kevent->key() == Qt::Key_Super_L)
                && getSingleMetaKeyPendingPressed(seat)) {
                setSingleMetaKeyPendingPressed(seat, false);
                return true;
            }
            if (kevent->key() == Qt::Key_Alt && m_taskSwitch) {
                if (m_taskSwitch->property("switchOn").toBool()) {
                    auto filter = Helper::instance()->workspace()->currentFilter();
                    filter->setFilterAppId("");
                }
                setCurrentMode(CurrentMode::Normal);
                QMetaObject::invokeMethod(m_taskSwitch, "exit");
            }
        }

        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            handleLeftButtonStateChanged(event);
        }

        if (event->type() == QEvent::Wheel) {
            handleWhellValueChanged(event);
        }
    }

    // NOTE: Unable to distinguish meta from other key combinations
    //       For example, Meta+S will still receive Meta release after
    //       the QML multitask view component is loaded, thus it would close the component
    if (event->type() == QEvent::KeyRelease) {
        auto kevent = static_cast<QKeyEvent *>(event);
        if (kevent->key() == Qt::Key_Meta) {
            if (m_taskSwitch)
                return true;
        }
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    doGesture(event);

    auto &seatState = m_seatMoveResizeStates[seat];
    if (seatState.surface) {
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            auto ownsOutput = seatState.surface->ownsOutput();
            if (!ownsOutput) {
                endMoveResizeForSeat(seat);
                return false;
            }

            auto increment_pos = ev->globalPosition() - seatState.initialPosition;
            doMoveResizeForSeat(seat, increment_pos);

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            endMoveResizeForSeat(seat);
        }
    }

    if (!getMoveResizeSurfaceForSeat(eventSeat) && m_rootSurfaceContainer->moveResizeSurface()) {
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            auto surface = m_rootSurfaceContainer->moveResizeSurface();
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

    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);

        if (Q_UNLIKELY(event->spontaneous())) {
            auto lastTimestamp = targetWindow->property("_lastEventTimestamp").value<qint64>();
            if (qAbs(static_cast<qint64>(kevent->timestamp()) - lastTimestamp) < 50) {
                return false;
            }
            targetWindow->setProperty("_lastEventTimestamp", static_cast<qint64>(kevent->timestamp()));
        }

        if (targetWindow->isActive() && m_renderWindow == targetWindow) {
            auto surface = keyboardFocusSurface();
            if (surface && surface != activatedSurface()) {
                this->activateSurface(surface, Qt::MouseFocusReason);
            }
        }
    }

    // handle shortcut
    if (seat == m_seat && !m_captureSelector && m_currentMode == CurrentMode::Normal
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        do {
            auto kevent = static_cast<QKeyEvent *>(event);
            // SKIP Meta+Meta
            if (kevent->key() == Qt::Key_Meta && kevent->modifiers() == Qt::NoModifier
                && !getSingleMetaKeyPendingPressed(seat)) {
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

bool Helper::afterHandleEvent(WSeat *seat,
                              WSurface *watched,
                              QObject *surfaceItem,
                              QObject *eventObject,
                              QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend) {
        return false;
    }
    if (event->isSinglePointEvent()) {
        if (static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        auto toplevelSurface = qobject_cast<WSurfaceItem *>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);

        auto surface = m_rootSurfaceContainer->getSurface(watched);

        WSeat *eventSeat = getSeatForEvent(event);
            if (eventSeat && surface) {

                for (auto seat : m_seatManager->seats()) {
                    if (seat != eventSeat && getMoveResizeSurfaceForSeat(seat)) {
                        endMoveResizeForSeat(seat);
                    }
                }

                setActivatedSurfaceForSeat(eventSeat, surface);
                if (surface->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus)
                    && surface->acceptKeyboardFocus()) {
                    if (eventSeat && eventSeat->nativeHandle()) {
                        updateSurfaceSeatInteraction(surface, eventSeat);

                        surface->setFocus(true, Qt::MouseFocusReason);

                        if (!surface->surface() || !eventSeat->nativeHandle()) {
                            qWarning() << "[INPUT] Invalid surface or seat native handle, skipping keyboard focus change";
                            return false;
                        }

                        if (!surface->surface() || !surface->surface()->handle()) {
                            qWarning() << "[INPUT] Surface is not valid for keyboard focus";
                            return false;
                        }

                        auto keyboard = eventSeat->keyboard();
                        if (!keyboard) {
                            qWarning() << "[INPUT] Seat has no keyboard device, skipping focus change";
                            return false;
                        }

                        surface->setProperty("lastInteractionTime", QDateTime::currentMSecsSinceEpoch());
                        eventSeat->setKeyboardFocusSurface(surface->surface());
                        m_seatKeyboardFocusSurfaces[eventSeat] = surface;
                    }
                }

                if (auto sh = surface->shellSurface();
                    sh && surface->surface() && surface->surface()->mapped()
                    && sh->hasCapability(WToplevelSurface::Capability::Focus)
                    && surface->hasActiveCapability()) {
                    surface->setActivate(true);
                    surface->stackToLast();
                    workspace()->pushActivedSurface(surface);

                    if (eventSeat == m_seat) {
                        setActivatedSurface(surface);
                    }
                }
            }
        }
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *seat, QWindow *, QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend) {
        return false;
    }
    if (event->isSinglePointEvent()) {
        if (static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
            WSeat *eventSeat = getSeatForEvent(event);
            if (eventSeat) {
            setActivatedSurfaceForSeat(eventSeat, nullptr);
                requestKeyboardFocusForSurfaceForSeat(eventSeat, nullptr, Qt::OtherFocusReason);
            }

            if (!eventSeat || eventSeat == m_seat) {
                activateSurface(nullptr, Qt::OtherFocusReason);
            }
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


void Helper::requestKeyboardFocusForSurfaceForSeat(WSeat *seat, SurfaceWrapper *newActivate, Qt::FocusReason reason)
{
    if (!seat || !seat->nativeHandle()) {
        qWarning() << "[FOCUS] Cannot set keyboard focus for null or invalid seat";
        return;
    }

    auto *nowKeyboardFocusSurface = getKeyboardFocusSurfaceForSeat(seat);
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

    if (newActivate) {
        updateSurfaceSeatInteraction(newActivate, seat);
    }

    bool isMainSeat = (seat == m_seat);

    if (nowKeyboardFocusSurface) {
        bool shouldClearUIFocus = isMainSeat;

        if (!isMainSeat) {
            bool otherSeatHasFocus = false;
            for (auto otherSeat : m_seatManager->seats()) {
                if (otherSeat != seat && getKeyboardFocusSurfaceForSeat(otherSeat) == nowKeyboardFocusSurface) {
                    otherSeatHasFocus = true;
                    break;
                }
            }

            shouldClearUIFocus = !otherSeatHasFocus;
        }

        if (shouldClearUIFocus) {
            nowKeyboardFocusSurface->setFocus(false, reason);
        }
    }

    if (newActivate) {
        m_seatKeyboardFocusSurfaces[seat] = newActivate;
    } else {
        m_seatKeyboardFocusSurfaces.remove(seat);
    }

    if (newActivate) {
        newActivate->setFocus(true, reason);
        seat->setKeyboardFocusSurface(newActivate->surface());
        updateSurfaceSeatInteraction(newActivate, seat);
    } else {
        seat->setKeyboardFocusSurface(nullptr);
    }
}

SurfaceWrapper *Helper::getKeyboardFocusSurfaceForSeat(WSeat *seat) const
{
    if (!seat) return nullptr;
    return m_seatKeyboardFocusSurfaces.value(seat);
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

    if (m_activatedSurface && m_activatedSurface->shellSurface())
        m_activatedSurface->setActivate(false);

    if (newActivateSurface) {
        if (m_showDesktop == WindowManagementV1::DesktopState::Show) {
            m_showDesktop = WindowManagementV1::DesktopState::Normal;
            m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Normal);
            newActivateSurface->setHideByShowDesk(true);
        }

        newActivateSurface->setActivate(true);
        if (auto sh = newActivateSurface->shellSurface();
            sh && newActivateSurface->surface() && newActivateSurface->surface()->mapped()) {
            workspace()->pushActivedSurface(newActivateSurface);
        }
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
        if (m_lockScreen && currentMode() == Helper::CurrentMode::Normal) {
            setCurrentMode(CurrentMode::LockScreen);
            m_lockScreen->shutdown();
            setWorkspaceVisible(false);
        }
    });
    connect(lockScreen, &LockScreenInterface::lock, this, [this]() {
        if (m_lockScreen && currentMode() == Helper::CurrentMode::Normal) {
            setCurrentMode(CurrentMode::LockScreen);
            m_lockScreen->lock();
            setWorkspaceVisible(false);
        }
    });
    connect(lockScreen, &LockScreenInterface::switchUser, this, [this]() {
        if (m_lockScreen && currentMode() == Helper::CurrentMode::Normal) {
            setCurrentMode(CurrentMode::LockScreen);
            m_lockScreen->switchUser();
            setWorkspaceVisible(false);
        }
    });
}


void Helper::onSessionNew(const QString &sessionId, const QDBusObjectPath &sessionPath)
{
    const auto path = sessionPath.path();
    qCDebug(qLcHelper) << "Session new, sessionId:" << sessionId << ", sessionPath:" << path;
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
        Output *o;
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

void Helper::setOutputProxy(Output *output) { }

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

void Helper::addSocket(WSocket *socket)
{
    m_server->addSocket(socket);
}

WXWayland *Helper::createXWayland()
{
    return m_shellHandler->createXWayland(m_server, m_seat, m_compositor, false);
}

void Helper::removeXWayland(WXWayland *xwayland)
{
    m_shellHandler->removeXWayland(xwayland);
}

WSocket *Helper::defaultWaylandSocket() const
{
    return m_socket;
}

WXWayland *Helper::defaultXWaylandSocket() const
{
    return m_defaultXWayland;
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

QString Helper::cursorTheme() const
{
    return TreelandConfig::ref().cursorThemeName();
}

QSize Helper::cursorSize() const
{
    return TreelandConfig::ref().cursorSize();
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
#ifndef DISABLE_DDM
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

        if (m_activatedSurface) {
            m_activatedSurface->setFocus(true, Qt::NoFocusReason);
        }
    });

    QDBusConnection::systemBus().connect("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "SessionNew", this, SLOT(onSessionNew(const QString &,const QDBusObjectPath &)));

    if (CmdLine::ref().useLockScreen()) {
        showLockScreen();
    }
#endif
}

void Helper::setCurrentMode(CurrentMode mode)
{
    if (m_currentMode == mode)
        return;

    TreelandConfig::ref().setBlockActivateSurface(mode != CurrentMode::Normal);

    m_currentMode = mode;

    Q_EMIT currentModeChanged();
}

void Helper::showLockScreen()
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
    QDBusInterface interface("org.freedesktop.DisplayManager",
                             "/org/freedesktop/DisplayManager/Seat0",
                             "org.freedesktop.DisplayManager.Seat",
                             QDBusConnection::systemBus());
    interface.asyncCall("SwitchToGreeter");
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

void Helper::assignDeviceToSeat(WInputDevice *device)
{
    if (!device) {
        qWarning() << "Cannot assign null device to seat";
        return;
    }

    QString deviceName = device->name();
    WInputDevice::Type deviceType = device->type();

    if (deviceType == WInputDevice::Type::Pointer && deviceName.contains("Keyboard", Qt::CaseInsensitive)) {
        qWarning() << "Device type mismatch - ignoring:" << deviceName;
        return;
    }

    if (deviceType == WInputDevice::Type::Keyboard &&
        (deviceName.contains("Mouse", Qt::CaseInsensitive) || deviceName.contains("Touchpad", Qt::CaseInsensitive))) {
        qWarning() << "Device type mismatch - ignoring:" << deviceName;
        return;
    }

    if (deviceType == WInputDevice::Type::Keyboard &&
        (deviceName.contains("Power Button") || deviceName.contains("Sleep Button") ||
         deviceName.contains("Lid Switch") || deviceName.contains("Video Bus"))) {
        return;
    }

    for (auto seat : m_seatManager->seats()) {
        if (seat->deviceList().contains(device)) {
            return;
        }
    }

    bool assigned = m_seatManager->autoAssignDevice(device);
    if (!assigned && m_seat && m_seat->nativeHandle()) {
        m_seat->attachInputDevice(device);
    }

    WSeat *assignedSeat = nullptr;
    for (auto seat : m_seatManager->seats()) {
        if (seat->deviceList().contains(device)) {
            assignedSeat = seat;
            break;
        }
    }

    if (!assignedSeat) {
        return;
    }

    if (!assignedSeat->nativeHandle()) {
        m_server->attach(assignedSeat);
    }

    if (deviceType == WInputDevice::Type::Pointer && !assignedSeat->cursor()) {
        WCursor *cursor = new WCursor(assignedSeat);
        cursor->setParent(assignedSeat);
        if (m_rootSurfaceContainer && m_rootSurfaceContainer->outputLayout()) {
            cursor->setLayout(m_rootSurfaceContainer->outputLayout());
        }
        assignedSeat->setCursor(cursor);
    }

    if (deviceType == WInputDevice::Type::Keyboard && !assignedSeat->keyboardFocusWindow()) {
        assignedSeat->setKeyboardFocusWindow(m_renderWindow);
    }
}

void Helper::checkAndFixSeatDevices()
{
    for (auto seat : m_seatManager->seats()) {
        if (!seat->nativeHandle()) {
            m_server->attach(seat);
        }
    }

    for (auto device : m_backend->inputDeviceList()) {
        bool isAssigned = false;
        for (auto seat : m_seatManager->seats()) {
            if (seat->deviceList().contains(device)) {
                isAssigned = true;
                break;
            }
        }

        if (!isAssigned) {
            assignDeviceToSeat(device);
        }
    }

    for (auto seat : m_seatManager->seats()) {
        bool hasPointer = false;
        bool hasKeyboard = false;

        for (auto device : seat->deviceList()) {
            if (device->type() == WInputDevice::Type::Pointer) {
                hasPointer = true;
            } else if (device->type() == WInputDevice::Type::Keyboard) {
                hasKeyboard = true;
            }
        }

        if (!hasPointer) {
            // Try to assign an unassigned pointer device to this seat
            for (auto device : m_backend->inputDeviceList()) {
                if (device->type() == WInputDevice::Type::Pointer) {
                    bool assigned = false;
                    for (auto s : m_seatManager->seats()) {
                        if (s->deviceList().contains(device)) {
                            assigned = true;
                            break;
                        }
                    }

                    if (!assigned) {
                        seat->attachInputDevice(device);
                        hasPointer = true;
                        break;
                    }
                }
            }
        }

        if (!hasKeyboard) {
            // Try to assign an unassigned keyboard device to this seat
            for (auto device : m_backend->inputDeviceList()) {
                if (device->type() == WInputDevice::Type::Keyboard) {
                    bool assigned = false;
                    for (auto s : m_seatManager->seats()) {
                        if (s != seat && s->deviceList().contains(device)) {
                            assigned = true;
                            break;
                        }
                    }

                    if (!assigned) {
                        seat->attachInputDevice(device);
                        hasKeyboard = true;
                        break;
                    }
                }
            }
        }

        if (!seat->cursor() && std::any_of(seat->deviceList().begin(), seat->deviceList().end(),
                                          [](WInputDevice* device) { return device->type() == WInputDevice::Type::Pointer; })) {
            WCursor *cursor = new WCursor(seat);
            cursor->setParent(seat);  // Fix memory leak by setting parent
            if (m_rootSurfaceContainer && m_rootSurfaceContainer->outputLayout()) {
                cursor->setLayout(m_rootSurfaceContainer->outputLayout());
            }
            seat->setCursor(cursor);
        }
    }
}

void Helper::setupSeatsConfiguration()
{
    for (auto seat : m_seatManager->seats()) {
        if (!seat->nativeHandle()) {
            m_server->attach(seat);
            if (!seat->nativeHandle()) {
                qWarning() << "Failed to create native handle for seat" << seat->name();
                continue;
            }
        }

        if (!seat->cursor()) {
            if (seat->name() == "seat0" && m_rootSurfaceContainer->cursor()) {
                seat->setCursor(m_rootSurfaceContainer->cursor());
            } else {
                WCursor *cursor = new WCursor(seat);
                cursor->setParent(seat);
                if (m_rootSurfaceContainer->outputLayout()) {
                    cursor->setLayout(m_rootSurfaceContainer->outputLayout());
                }
                seat->setCursor(cursor);
            }
        }

        seat->setKeyboardFocusWindow(m_renderWindow);

        if (!seat->eventFilter()) {
            seat->setEventFilter(this);
        }

        disconnect(seat, &WSeat::requestDrag, this, nullptr);
        connect(seat, &WSeat::requestDrag, this, [this, seat](WSurface *surface) {
            handleRequestDragForSeat(seat, surface);
        });
    }
}

void Helper::connectDeviceSignals()
{
    connect(m_backend, &WBackend::inputAdded, this, [this](WInputDevice *device) {
        if (!device)
            return;

        assignDeviceToSeat(device);

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
}

void Helper::assignExistingDevices()
{
    for (auto device : m_backend->inputDeviceList()) {
        assignDeviceToSeat(device);
    }
}

bool Helper::getSingleMetaKeyPendingPressed(WSeat *seat) const
{
    return m_seatMetaKeyStates.value(seat, false);
}

void Helper::setSingleMetaKeyPendingPressed(WSeat *seat, bool pressed)
{
    if (!seat)
        return;

    if (pressed) {
        m_seatMetaKeyStates[seat] = true;
    } else {
        m_seatMetaKeyStates.remove(seat);
    }
}

WSeat *Helper::getSeatForEvent(QInputEvent *event) const
{
    if (!event) {
        qWarning() << "getSeatForEvent called with null event";
        return nullptr;  // Return nullptr to let caller handle error
    }

    if (event->device()) {
        WInputDevice *device = WInputDevice::from(event->device());
        if (device) {
            WSeat *deviceSeat = device->seat();
            if (deviceSeat) {
                return deviceSeat;
            }
        }
    }

    return m_seat;
}

WSeat *Helper::findSeatForSurface(SurfaceWrapper *wrapper) const
{
    if (!wrapper)
        return nullptr;

    for (auto seat : m_seatManager->seats()) {
        auto focusSurface = seat->pointerFocusSurface();
        if (focusSurface && focusSurface->handle()->handle() == wrapper->surface()->handle()->handle()) {
            return seat;
        }

        auto activatedSurface = getActivatedSurfaceForSeat(seat);
        if (activatedSurface == wrapper) {
            return seat;
        }
    }

    return getLastInteractingSeat(wrapper);
}

void Helper::setActivatedSurfaceForSeat(WSeat *seat, SurfaceWrapper *surface)
{
    if (!seat)
        return;

    auto currentSurface = m_seatActivatedSurfaces.value(seat);
    if (currentSurface == surface)
        return;

    if (currentSurface && currentSurface->shellSurface()) {
        if (!(surface && surface->type() == SurfaceWrapper::Type::XWayland &&
              surface->shellSurface() && !surface->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus))) {
            currentSurface->setActivate(false);
        }
    }

    if (surface) {
        surface->setActivate(true);
        surface->stackToLast();
        if (auto sh = surface->shellSurface(); sh && surface->surface() && surface->surface()->mapped()) {
            workspace()->pushActivedSurface(surface);
        }
    }

    m_seatActivatedSurfaces[seat] = surface;

    if (seat == m_seat) {
        m_activatedSurface = surface;
        Q_EMIT activatedSurfaceChanged();
    }
}

SurfaceWrapper *Helper::getActivatedSurfaceForSeat(WSeat *seat) const
{
    if (!seat)
        return nullptr;
    return m_seatActivatedSurfaces.value(seat);
}

void Helper::handleRequestDragForSeat(WSeat *seat, WSurface *surface)
{
    if (!seat || !seat->nativeHandle())
        return;

    seat->setAlwaysUpdateHoverTarget(true);
    struct wlr_drag *drag = seat->nativeHandle()->drag;
    Q_ASSERT(drag);

    QObject::connect(qw_drag::from(drag), &qw_drag::notify_drop, this, [this, seat] {
        if (m_ddeShellV1)
            DDEActiveInterface::sendDrop(seat);
    });

    QObject::connect(qw_drag::from(drag), &qw_drag::before_destroy, this, [this, seat, drag] {
        drag->data = NULL;
        seat->setAlwaysUpdateHoverTarget(false);
    });

    if (m_ddeShellV1)
        DDEActiveInterface::sendStartDrag(seat);
}

void Helper::beginMoveResizeForSeat(WSeat *seat, SurfaceWrapper *surface, Qt::Edges edges)
{
    if (!seat || !surface)
        return;

    if (m_seatMoveResizeStates[seat].surface && m_seatMoveResizeStates[seat].surface != surface) {
        endMoveResizeForSeat(seat);
    }

    if (m_rootSurfaceContainer->moveResizeSurface() != nullptr) {
        m_rootSurfaceContainer->endMoveResize();
    }


    auto &seatState = m_seatMoveResizeStates[seat];
    seatState.surface = surface;
    seatState.edges = edges;
    seatState.startGeometry = surface->geometry();
    seatState.initialPosition = seat->cursor() ? seat->cursor()->position() : QPointF();
}

void Helper::endMoveResizeForSeat(WSeat *seat)
{
    if (!seat)
        return;

    if (m_seatMoveResizeStates.contains(seat)) {
        m_seatMoveResizeStates.remove(seat);
    }
}

SurfaceWrapper *Helper::getMoveResizeSurfaceForSeat(WSeat *seat) const
{
    if (!seat)
        return nullptr;
    return m_seatMoveResizeStates.value(seat).surface;
}

void Helper::doMoveResizeForSeat(WSeat *seat, const QPointF &incrementPos)
{
    if (!seat)
        return;

    auto &seatState = m_seatMoveResizeStates[seat];
    if (!seatState.surface)
        return;

    if (seatState.edges == Qt::Edges{}) {
        // Move operation
        auto newPos = seatState.startGeometry.topLeft() + incrementPos;
        seatState.surface->setPosition(newPos);
    } else {

        QRectF geo = seatState.startGeometry;

        if (seatState.edges & Qt::LeftEdge)
            geo.setLeft(geo.left() + incrementPos.x());
        if (seatState.edges & Qt::TopEdge)
            geo.setTop(geo.top() + incrementPos.y());
        if (seatState.edges & Qt::RightEdge)
            geo.setRight(geo.right() + incrementPos.x());
        if (seatState.edges & Qt::BottomEdge)
            geo.setBottom(geo.bottom() + incrementPos.y());


        seatState.surface->resize(geo.size());
    }
}

WSeat *Helper::getLastInteractingSeat(SurfaceWrapper *surface) const
{
    if (!surface)
        return nullptr;

    auto lastSeatVariant = surface->property("lastInteractingSeat");
    if (lastSeatVariant.isValid()) {
        auto seat = lastSeatVariant.value<WSeat*>();

        if (m_seatManager->seats().contains(seat)) {
            return seat;
        }
    }
    return nullptr;
}

void Helper::updateSurfaceSeatInteraction(SurfaceWrapper *surface, WSeat *seat)
{
    if (!surface || !seat)
        return;

    surface->setProperty("lastInteractingSeat", QVariant::fromValue(seat));
    surface->setProperty("lastInteractionTime", QDateTime::currentMSecsSinceEpoch());
}

void Helper::switchWorkspaceForSeat(WSeat *seat, int index)
{
    if (!seat)
        return;
    workspace()->switchTo(index);
}

void Helper::toggleFpsDisplay()
{
    if (m_fpsDisplayManager) {
        m_fpsDisplayManager->toggle();
        qCInfo(qLcHelper) << "FPS display toggled, now" << (m_fpsDisplayManager->isVisible() ? "visible" : "hidden");
    } else {
        qCWarning(qLcHelper) << "FPS display manager not initialized";
    }
}

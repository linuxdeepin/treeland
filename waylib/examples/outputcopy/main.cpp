// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wlogging.h>
#include "helper.h"

#include <WServer>
#include <WOutput>
#include <WSeat>
#include <WBackend>
#include <wquickoutputlayout.h>
#include <wrenderhelper.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wcursor.h>
#include <woutputitem.h>
#include <woutputlayer.h>
#include <wquicktextureproxy.h>

#include <wlr_all.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

Helper::Helper(QObject *parent)
    : QObject(parent)
    , m_server(new WServer(this))
    , m_outputLayout(new WQuickOutputLayout(m_server))
    , m_cursor(new WCursor(this))
{
    m_renderWindow = new WOutputRenderWindow();
    m_renderWindow->setColor(Qt::black);

    m_seat = m_server->attach<WSeat>();
    m_seat->setCursor(m_cursor);
    m_cursor->setLayout(m_outputLayout);
}

Helper::~Helper() {
    m_renderWindow->deleteLater();
}

void Helper::initProtocols(QQmlEngine *qmlEngine)
{
    m_backend = m_server->attach<WBackend>();
    m_server->start();

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());

    if (!m_renderer) {
        qFatal("Failed to create renderer");
    }

    connect(m_backend, &WBackend::outputAdded, this, [this, qmlEngine] (WOutput *output) {
        if (!m_primaryOutput) {
            auto component = new QQmlComponent(qmlEngine, "OutputCopy", "PrimaryOutputDelegate",
                                               QQmlComponent::PreferSynchronous, this);
            auto obj = component->createWithInitialProperties({
                {"parent", QVariant::fromValue(m_renderWindow->contentItem())},
                {"waylandOutput", QVariant::fromValue(output)},
                {"layout", QVariant::fromValue(m_outputLayout)},
            });
            m_primaryOutput = qobject_cast<WOutputItem*>(obj);
            // ensure following this to destroy, because QQuickItem::setParentItem
            // is not auto add the child item to QObject's children.
            m_primaryOutput->setParent(this);
            Q_ASSERT(m_primaryOutput);

            m_primaryOutputViewport = m_primaryOutput->findChild<WOutputViewport*>({}, Qt::FindDirectChildrenOnly);
            updatePrimaryOutputHardwareLayers();
            connect(m_primaryOutputViewport, &WOutputViewport::hardwareLayersChanged,
                    this, &Helper::updatePrimaryOutputHardwareLayers);
        } else {
            auto component = new QQmlComponent(qmlEngine, "OutputCopy", "CopyOutputDelegate",
                                               QQmlComponent::PreferSynchronous, this);
            auto obj = component->createWithInitialProperties({
                {"parent", QVariant::fromValue(m_primaryOutput)},
                {"output", QVariant::fromValue(output)},
                {"targetOutputItem", QVariant::fromValue(m_primaryOutput)},
                {"targetViewport", QVariant::fromValue(m_primaryOutputViewport.get())},
            });
            // ensure following this to destroy, because QQuickItem::setParentItem
            // is not auto add the child item to QObject's children.
            obj->setParent(this);
            auto viewport = obj->findChild<WOutputViewport*>({}, Qt::FindDirectChildrenOnly);
            Q_ASSERT(viewport);
            auto textureProxy = obj->findChild<WQuickTextureProxy*>();
            Q_ASSERT(textureProxy);

            m_copyOutputs << std::make_pair(viewport, textureProxy);

            Q_ASSERT(m_primaryOutputViewport);
            // copy layers
            for (auto layer : std::as_const(m_hardwareLayersOfPrimaryOutput))
                m_renderWindow->attach(layer, viewport, m_primaryOutputViewport, textureProxy);

            // copy primary output
            m_primaryOutputViewport->setCacheBuffer(true);
        }
    });

    connect(m_backend, &WBackend::outputRemoved, this, [this] (WOutput *output) {
        if (m_primaryOutput->output() == output) {
            m_primaryOutput->setParent(nullptr);
            delete m_primaryOutput;
            m_copyOutputs.clear();
        } else {
            for (int i = 0; i < m_copyOutputs.size(); ++i) {
                WOutputViewport *viewport = m_copyOutputs[i].first;
                if (viewport->output() == output) {
                    m_copyOutputs.removeAt(i);
                    auto output = viewport->parent();
                    output->setParent(nullptr);
                    delete output;
                    break;
                }
            }
        }
    });

    connect(m_backend, &WBackend::inputAdded, this, [this] (WInputDevice *device) {
        m_seat->attachInputDevice(device);
    });

    connect(m_backend, &WBackend::inputRemoved, this, [this] (WInputDevice *device) {
        m_seat->detachInputDevice(device);
    });

    m_allocator = wlr_allocator_autocreate(m_backend->handle(), m_renderer);
    wlr_renderer_init_wl_display(m_renderer, m_server->handle());

    // free follow display
    m_compositor = wlr_compositor_create(m_server->handle(), 6, m_renderer);
    wlr_subcompositor_create(m_server->handle());

    connect(m_renderWindow, &WOutputRenderWindow::outputViewportInitialized, this, [] (WOutputViewport *viewport) {
        // Trigger wlr_output::frame signal in order to ensure WOutputHelper::renderable
        // property is true, OutputRenderWindow when will render this output in next frame.
        {
            WOutput *output = viewport->output();

            // Enable on default
            auto *wlrOutput = output->handle();
            // Don't care for WOutput::isEnabled, must do WOutput::commit here,
            // In order to ensure trigger qw_output::frame signal, WOutputRenderWindow
            // needs this signal to render next frmae. Because qw_output::frame signal
            // maybe Q_EMIT before WOutputRenderWindow::attach, if no commit here,
            // WOutputRenderWindow will ignore this ouptut on render.
            if (!output->property("_Enabled").toBool()) {
                output->setProperty("_Enabled", true);
                wlr_output_state newState;
                wlr_output_state_init(&newState);

                if (!wlrOutput->current_mode) {
                    auto mode = wlr_output_preferred_mode(wlrOutput);
                    if (mode)
                        wlr_output_state_set_mode(&newState, mode);
                }
                wlr_output_state_set_enabled(&newState, true);
                if (!wlr_output_commit_state(wlrOutput, &newState)) {
                    qCritical("commit failed on output %s", wlrOutput->name);
                }
                wlr_output_state_finish(&newState);
            }
        }
    });
    m_renderWindow->init(m_renderer, m_allocator);

    wlr_backend_start(m_backend->handle());
}

void Helper::updatePrimaryOutputHardwareLayers()
{
    const auto layers = m_primaryOutputViewport->hardwareLayers();
    for (auto layer : layers) {
        if (m_hardwareLayersOfPrimaryOutput.removeOne(layer))
            continue;
        for (auto copyOutput : std::as_const(m_copyOutputs))
            m_renderWindow->attach(layer, copyOutput.first,
                                   m_primaryOutputViewport, copyOutput.second);
    }

    for (auto oldLayer : std::as_const(m_hardwareLayersOfPrimaryOutput)) {
        for (auto copyOutput : std::as_const(m_copyOutputs))
            m_renderWindow->detach(oldLayer, copyOutput.first);
    }

    m_hardwareLayersOfPrimaryOutput = layers;
}

int main(int argc, char *argv[]) {
    WLog::init();
    WServer::initializeQPA();
//    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine waylandEngine;

    Helper *helper = waylandEngine.singletonInstance<Helper*>("OutputCopy", "Helper");
    Q_ASSERT(helper);

    helper->initProtocols(&waylandEngine);

    // multi output
    wlr_multi_for_each_backend(helper->backend()->handle(), [] (wlr_backend *backend, void *) {
        wlr_output *newOutput = nullptr;

        if (wlr_backend_is_x11(backend)) {
            newOutput = wlr_x11_output_create(backend);
        } else if (wlr_backend_is_wl(backend)) {
            newOutput = wlr_wl_output_create(backend);
       }

       if (!newOutput)
           return;

       // 800x600
        wlr_output_state newState;
        wlr_output_state_init(&newState);
        wlr_output_state_set_custom_mode(&newState, 1000, 600, 0);
        wlr_output_commit_state(newOutput, &newState);
        wlr_output_state_finish(&newState);
    }, nullptr);

    return app.exec();
}

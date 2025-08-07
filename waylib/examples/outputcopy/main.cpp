// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

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

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwlogging.h>
#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwcompositor.h>
#include <qwrenderer.h>
#include <qwallocator.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

QW_USE_NAMESPACE

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

    m_allocator = qw_allocator::autocreate(*m_backend->handle(), *m_renderer);
    m_renderer->init_wl_display(*m_server->handle());

    // free follow display
    m_compositor = qw_compositor::create(*m_server->handle(), 6, *m_renderer);
    qw_subcompositor::create(*m_server->handle());

    connect(m_renderWindow, &WOutputRenderWindow::outputViewportInitialized, this, [] (WOutputViewport *viewport) {
        // Trigger qw_output::frame signal in order to ensure WOutputHelper::renderable
        // property is true, OutputRenderWindow when will render this output in next frame.
        {
            WOutput *output = viewport->output();

            // Enable on default
            auto qwoutput = output->handle();
            // Don't care for WOutput::isEnabled, must do WOutput::commit here,
            // In order to ensure trigger qw_output::frame signal, WOutputRenderWindow
            // needs this signal to render next frmae. Because qw_output::frame signal
            // maybe Q_EMIT before WOutputRenderWindow::attach, if no commit here,
            // WOutputRenderWindow will ignore this ouptut on render.
            if (!qwoutput->property("_Enabled").toBool()) {
                qwoutput->setProperty("_Enabled", true);
                qw_output_state newState;

                if (!qwoutput->handle()->current_mode) {
                    auto mode = qwoutput->preferred_mode();
                    if (mode)
                        newState.set_mode(mode);
                }
                newState.set_enabled(true);
                bool ok = qwoutput->commit_state(newState);
                Q_ASSERT(ok);
            }
        }
    });
    m_renderWindow->init(m_renderer, m_allocator);

    m_backend->handle()->start();
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
    qw_log::init();
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
    qobject_cast<qw_multi_backend*>(helper->backend()->handle())->for_each_backend([] (wlr_backend *backend, void *) {
        qw_output *newOutput = nullptr;

       if (auto x11 = qw_x11_backend::from(backend)) {
           newOutput = qw_output::from(x11->output_create());
       } else if (auto wayland = qw_wayland_backend::from(backend)) {
           newOutput = qw_output::from(wayland->output_create());
       }

       if (!newOutput)
           return;

       // 800x600
       qw_output_state newState;
       newState.set_custom_mode(1000, 600, 0);
       newOutput->commit_state(newState);
    }, nullptr);

    return app.exec();
}

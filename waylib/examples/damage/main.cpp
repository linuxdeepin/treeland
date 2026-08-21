// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wlogging.h>
#include "helper.h"

#include <WServer>
#include <WOutput>
#include <WSeat>
#include <WBackend>
#include <wquickcursor.h>
#include <wquickoutputlayout.h>
#include <wrenderhelper.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>

#include <wlr_all.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickWindow>

Helper::Helper(QObject *parent)
    : QObject(parent)
    , m_server(new WServer(this))
    , m_outputCreator(new WQmlCreator(this))
    , m_outputLayout(new WQuickOutputLayout(m_server))
    , m_cursor(new WCursor(this))
{
    m_seat = m_server->attach<WSeat>();
    m_seat->setCursor(m_cursor);
    m_cursor->setLayout(m_outputLayout);
}

QString Helper::damageDebugMode() const
{
    return WRenderHelper::damageDebugMode();
}

void Helper::setDamageDebugMode(const QString &mode)
{
    if (mode == damageDebugMode())
        return;
    if (WRenderHelper::setDamageDebugMode(mode))
        Q_EMIT damageDebugModeChanged();
}

void Helper::initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine)
{
    m_backend = m_server->attach<WBackend>();
    m_server->start();

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());

    if (!m_renderer) {
        qFatal("Failed to create renderer");
    }

    connect(m_backend, &WBackend::outputAdded, this, [this, qmlEngine] (WOutput *output) {
        auto initProperties = qmlEngine->newObject();
        initProperties.setProperty("waylandOutput", qmlEngine->toScriptValue(output));
        initProperties.setProperty("layout", qmlEngine->toScriptValue(m_outputLayout));
        initProperties.setProperty("x", qmlEngine->toScriptValue(m_outputLayout->implicitWidth()));

        m_outputCreator->add(output, initProperties);
    });

    connect(m_backend, &WBackend::outputRemoved, this, [this] (WOutput *output) {
        m_outputCreator->removeByOwner(output);
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

    connect(window, &WOutputRenderWindow::outputViewportInitialized, this, [] (WOutputViewport *viewport) {
        // Trigger QWOutput::frame signal in order to ensure WOutputHelper::renderable
        // property is true, OutputRenderWindow when will render this output in next frame.
        {
            WOutput *output = viewport->output();

            // Enable on default
            auto *wlrOutput = output->handle();
            // Don't care for WOutput::isEnabled, must do WOutput::commit here,
            // In order to ensure trigger QWOutput::frame signal, WOutputRenderWindow
            // needs this signal to render next frmae. Because QWOutput::frame signal
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
    window->init(m_renderer, m_allocator);

    wlr_backend_start(m_backend->handle());
}

int main(int argc, char *argv[])
{
    WLog::init();
    WServer::initializeQPA();
    QQuickStyle::setStyle("Fusion");

    // Default-on dirty-region overlay. Honor an explicit WAYLIB_DEBUG_DAMAGE.
    if (qEnvironmentVariableIsEmpty("WAYLIB_DEBUG_DAMAGE"))
        WRenderHelper::setDamageDebugMode(QStringLiteral("highlight"));

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine waylandEngine;
    waylandEngine.loadFromModule("DamagePlayground", "Main");

    auto window = waylandEngine.rootObjects().first()->findChild<WOutputRenderWindow *>();
    Q_ASSERT(window);

    Helper *helper = waylandEngine.singletonInstance<Helper *>("DamagePlayground", "Helper");
    Q_ASSERT(helper);

    helper->initProtocols(window, &waylandEngine);

    return app.exec();
}

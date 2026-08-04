// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "TestHelper.h"

#include <WServer>
#include <WOutput>
#include <WSeat>
#include <WBackend>
#include <wquickcursor.h>
#include <wquickoutputlayout.h>
#include <wrenderhelper.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>

extern "C" {
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_subcompositor.h>
}


extern "C" {
#define static
#include <wlr/render/pixman.h>
#undef static
}
TestHelper::TestHelper(QObject *parent)
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

void TestHelper::initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine)
{
    m_backend = m_server->attach<WBackend>();
    m_server->start();

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());
    if (!m_renderer)
        qFatal("Failed to create wlroots renderer");

    connect(m_backend, &WBackend::outputAdded, this, [this, qmlEngine](WOutput *output) {
        auto props = qmlEngine->newObject();
        props.setProperty("waylandOutput", qmlEngine->toScriptValue(output));
        props.setProperty("layout", qmlEngine->toScriptValue(m_outputLayout));
        props.setProperty("x", qmlEngine->toScriptValue(m_outputLayout->implicitWidth()));
        m_outputCreator->add(output, props);
    });

    connect(m_backend, &WBackend::outputRemoved, this, [this](WOutput *output) {
        m_outputCreator->removeByOwner(output);
    });

    connect(m_backend, &WBackend::inputAdded, this, [this](WInputDevice *device) {
        m_seat->attachInputDevice(device);
    });

    connect(m_backend, &WBackend::inputRemoved, this, [this](WInputDevice *device) {
        m_seat->detachInputDevice(device);
    });

    m_allocator = wlr_allocator_autocreate(m_backend->handle(), m_renderer);
    wlr_renderer_init_wl_display(m_renderer, m_server->handle());

    m_compositor = wlr_compositor_create(m_server->handle(), 6, m_renderer);
    wlr_subcompositor_create(m_server->handle());

    connect(window, &WOutputRenderWindow::outputViewportInitialized, this, [](WOutputViewport *viewport) {
        auto *output = viewport->output();
        auto *nativeOutput = output->handle();
        if (!output->property("_Enabled").toBool()) {
            output->setProperty("_Enabled", true);
            wlr_output_state newState;
            wlr_output_state_init(&newState);
            if (!nativeOutput->current_mode) {
                auto mode = wlr_output_preferred_mode(nativeOutput);
                if (mode)
                    wlr_output_state_set_mode(&newState, mode);
            }
            wlr_output_state_set_enabled(&newState, true);
            if (!wlr_output_commit_state(nativeOutput, &newState))
                qCritical("commit failed on output %s", nativeOutput->name);
            wlr_output_state_finish(&newState);
        }
    });

    window->init(m_renderer, m_allocator);
    wlr_backend_start(m_backend->handle());
}

bool TestHelper::usesSoftwareRenderer() const
{
    return m_renderer && wlr_renderer_is_pixman(m_renderer);
}

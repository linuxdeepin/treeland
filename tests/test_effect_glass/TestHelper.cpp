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

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwrenderer.h>
#include <qwallocator.h>


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

    m_allocator = qw_allocator::autocreate(*m_backend->handle(), *m_renderer);
    m_renderer->init_wl_display(*m_server->handle());

    m_compositor = qw_compositor::create(*m_server->handle(), 6, *m_renderer);
    qw_subcompositor::create(*m_server->handle());

    connect(window, &WOutputRenderWindow::outputViewportInitialized, this, [](WOutputViewport *viewport) {
        auto qwoutput = viewport->output()->handle();
        if (!qwoutput->property("_Enabled").toBool()) {
            qwoutput->setProperty("_Enabled", true);
            qw_output_state newState;
            if (!qwoutput->handle()->current_mode) {
                auto mode = qwoutput->preferred_mode();
                if (mode)
                    newState.set_mode(mode);
            }
            newState.set_enabled(true);
            if (!qwoutput->commit_state(newState))
                qCritical("commit failed on output %s", qwoutput->handle()->name);
        }
    });

    window->init(m_renderer, m_allocator);
    m_backend->handle()->start();
}

bool TestHelper::usesSoftwareRenderer() const
{
    return m_renderer && wlr_renderer_is_pixman(m_renderer->handle());
}

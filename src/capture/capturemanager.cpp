// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "capturemanager.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "modules/capture/capture.h"

#include <wserver.h>
#include <woutputrenderwindow.h>

CaptureManager::CaptureManager(WServer *server, RootSurfaceContainer *root, QObject *parent)
    : QObject(parent)
    , m_rootSurfaceContainer(root)
    , m_server(server)
{}

CaptureManager::~CaptureManager()
{
    teardown();
}

void CaptureManager::installProtocol(QmlEngine *engine, WOutputRenderWindow *window)
{
    m_engine = engine;
    m_captureManagerV1 = m_server->attach<CaptureManagerV1>();
    m_captureManagerV1->setOutputRenderWindow(window);
    connect(m_captureManagerV1, &CaptureManagerV1::contextInSelectionChanged,
            this, &CaptureManager::onSelectionChanged);
}

void CaptureManager::onSelectionChanged()
{
    if (m_captureManagerV1->contextInSelection()) {
        m_captureSelector = qobject_cast<CaptureSourceSelector *>(
            m_engine->createCaptureSelector(m_rootSurfaceContainer, m_captureManagerV1));
    } else if (m_captureSelector) {
        m_captureSelector->deleteLater();
    }
}

bool CaptureManager::isSelecting() const
{
    return m_captureSelector;
}

void CaptureManager::cancelSelection()
{
    if (m_captureSelector)
        m_captureSelector->cancelSelection();
}

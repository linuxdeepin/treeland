// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualclient.h"
#include "virtualoutput.h"
#include "virtualoutputmanager.h"

#include <private/qwaylandwindow_p.h>

#include <QApplication>
#include <QPushButton>
#include <QWidget>
#include <QtWaylandClient/QWaylandClientExtension>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <qlogging.h>

VirtualClient::VirtualClient(int argc, char *argv[], QObject *parent)
    : QObject(parent)
    , m_argc(argc)
    , m_argv(argv)
{
    m_manager = std::make_unique<VirtualOutputManager>();

    if (m_argc >= 3) {
        for (int i = 1; i < m_argc; ++i) {
            m_screenNames.append(QString::fromUtf8(m_argv[i]));
        }
    }

    connect(m_manager.get(), &VirtualOutputManager::activeChanged, this, [this]() {
        if (m_manager->isActive()) {
            setupUi();
        }
    });
}

VirtualClient::~VirtualClient() = default;

bool VirtualClient::isValid() const
{
    return m_manager && m_manager->isInitialized();
}

void VirtualClient::setupUi()
{
    m_widget = new QWidget;
    m_widget->setAttribute(Qt::WA_TranslucentBackground);
    m_widget->resize(640, 480);

    m_createButton = new QPushButton("Create virtual output", m_widget);
    m_createButton->setGeometry(0, 0, 150, 50);

    m_restoreButton = new QPushButton("Restore settings", m_widget);
    m_restoreButton->setGeometry(0, 60, 150, 50);

    m_listButton = new QPushButton("Get virtual output list", m_widget);
    m_listButton->setGeometry(0, 120, 150, 50);

    connect(m_createButton, &QPushButton::clicked, this, &VirtualClient::onCreateVirtualOutput);
    connect(m_restoreButton, &QPushButton::clicked, this, &VirtualClient::onRestoreSettings);
    connect(m_listButton, &QPushButton::clicked, this, &VirtualClient::onGetVirtualOutputList);

    connect(m_manager.get(),
            &VirtualOutputManager::virtualOutputListReceived,
            this,
            &VirtualClient::onVirtualOutputListReceived);

    m_widget->show();

    // Collect available screens
    QWindow *window = m_widget->windowHandle();
    QList<QtWaylandClient::QWaylandScreen *> screens;
    if (window && window->handle()) {
        QtWaylandClient::QWaylandWindow *waylandWindow =
            static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
        screens = waylandWindow->display()->screens();

        qInfo() << "Available screens:";
        for (auto *screen : screens) {
            qInfo() << "  Screen name:" << screen->name();
        }
    }
}

void VirtualClient::onVirtualOutputListReceived(const QStringList &names){
    qInfo() << names;
    for (const auto &name : names) {
        auto *obj = m_manager->getVirtualOutput(name);
        if (!obj) {
            qWarning() << "  Failed to get virtual output for:" << name;
            continue;
        }

        auto *vo = new VirtualOutput(obj);

        // Save as the current virtual output
        m_currentOutput.reset(vo);
    }
}

void VirtualClient::onCreateVirtualOutput()
{
    // If already has a virtual output, do nothing
    if (m_currentOutput && m_currentOutput->isInitialized()) {
        qInfo() << "Virtual output already exists, skipping creation";
        return;
    }

    if (m_screenNames.isEmpty()) {
        qInfo() << "No screens available to create virtual output";
        return;
    }

    // 实际使用需要判断主屏（被镜像的屏幕），将主屏放在wl_array的第一个,复制屏幕依次填充
    QByteArray screenNameArray;
    for (auto screen : m_screenNames) {
        screenNameArray.append(screen.toUtf8());
        screenNameArray.append('\0');
    }
    screenNameArray.append('\0');

    m_currentOutput.reset(
        new VirtualOutput(m_manager->createVirtualOutput("copyscreen1", screenNameArray)));

    qInfo() << "Virtual output created with screens:" << m_screenNames;
}

void VirtualClient::onRestoreSettings()
{
    if (m_currentOutput && m_currentOutput->isInitialized()) {
        m_currentOutput->destroy();
        m_currentOutput.reset();
        qInfo() << "Virtual output destroyed via restore";
    }
}

void VirtualClient::onGetVirtualOutputList()
{
    if (m_currentOutput && m_currentOutput->isInitialized()) {
        qInfo() << "Use already exit outputs: %s" << m_screenNames;
        return;
    }
    m_manager->getVirtualOutputList();
}

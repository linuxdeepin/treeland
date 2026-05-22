// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QObject>
#include <QSharedPointer>
#include <qcontainerfwd.h>

#include <memory>

class QWidget;
class QPushButton;
class VirtualOutputManager;
class VirtualOutput;

class VirtualClient : public QObject
{
    Q_OBJECT
public:
    explicit VirtualClient(int argc, char *argv[], QObject *parent = nullptr);
    ~VirtualClient();

    bool isValid() const;

private:
    void setupUi();
    void onCreateVirtualOutput();
    void onRestoreSettings();
    void onGetVirtualOutputList();
    void onVirtualOutputListReceived(const QStringList &names);

    std::unique_ptr<VirtualOutputManager> m_manager;
    QSharedPointer<VirtualOutput> m_currentOutput;
    QWidget *m_widget = nullptr;
    QPushButton *m_createButton = nullptr;
    QPushButton *m_restoreButton = nullptr;
    QPushButton *m_listButton = nullptr;
    QStringList m_screenNames;
    int m_argc = 0;
    char **m_argv = nullptr;
};

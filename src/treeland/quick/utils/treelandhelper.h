// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "helper.h"

WAYLIB_SERVER_USE_NAMESPACE

class WaylandSocketProxy;

class TreeLandHelper : public Helper {
    Q_OBJECT
    Q_PROPERTY(QString socketFile READ socketFile WRITE setSocketFile NOTIFY socketFileChanged FINAL)
    Q_PROPERTY(QString currentUser WRITE setCurrentUser FINAL)

    QML_ELEMENT
    QML_SINGLETON

public:
    explicit TreeLandHelper(QObject *parent = nullptr);

    enum Switcher {
        Hide,
        Show,
        Next,
        Previous,
    };
    Q_ENUM(Switcher)

    void setCurrentUser(const QString &currentUser);

    QString socketFile() const;

    Q_INVOKABLE QString clientName(Waylib::Server::WSurface *surface) const;

    bool addAction(const QString &user, QAction *action);
    void removeAction(const QString &user, QAction *action);

Q_SIGNALS:
    void socketFileChanged();
    void switcherChanged(Switcher mode);

protected:
    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;

private:
    void setSocketFile(const QString &socketFile);

private:
    QString m_socketFile;
    QString m_currentUser;
    Switcher m_switcherCurrentMode = Switcher::Hide;
    std::map<QString, std::vector<QAction*>> m_actions;
};

// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "helper.h"

WAYLIB_SERVER_USE_NAMESPACE

class TreeLandHelper : public Helper {
    Q_OBJECT
    Q_PROPERTY(QString socketFile READ socketFile WRITE setSocketFile NOTIFY socketFileChanged FINAL)

    QML_ELEMENT
    QML_SINGLETON

public:
    explicit TreeLandHelper(QObject *parent = nullptr);

    QString socketFile() const;

    Q_INVOKABLE QString clientName(Waylib::Server::WSurface *surface) const;

Q_SIGNALS:
    void keyEvent(uint32_t key, uint32_t modify);
    void socketFileChanged();

protected:
    bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) override;

private:
    void setSocketFile(const QString &socketFile);

private:
    QString m_socketFile;
};

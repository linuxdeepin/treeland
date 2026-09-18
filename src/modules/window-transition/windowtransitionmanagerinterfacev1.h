// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

class SurfaceWrapper;
class WindowTransitionManagerInterfaceV1Private;

class WindowTransitionManagerInterfaceV1
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit WindowTransitionManagerInterfaceV1(QObject *parent = nullptr);
    ~WindowTransitionManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

    void takeCommittedRect(const QString &token, wl_resource *tokenResource);
    void discardPendingRect(const QString &token);
    bool hasPendingWindowTransitionRect(const QString &token) const;
    bool associatePendingRect(const QString &token,
                              SurfaceWrapper *targetWrapper,
                              SurfaceWrapper *originWrapper);

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    friend class WindowTransitionRectV1;
    std::unique_ptr<WindowTransitionManagerInterfaceV1Private> d;
    QTimer m_pendingSweepTimer;
    void ensurePendingSweepRunning();
    void sweepPendingRects();
};

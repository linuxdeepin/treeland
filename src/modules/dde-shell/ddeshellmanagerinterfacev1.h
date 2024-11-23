// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>
#include <wsurface.h>
#include <wserver.h>

#include <QObject>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class DDEShellManagerInterfaceV1Private;
class DDEShellSurfaceInterface;
class DDEActiveInterface;
class WindowOverlapCheckerInterface;
class MultiTaskViewInterface;
class WindowPickerInterface;
class LockScreenInterface;

class DDEShellManagerInterfaceV1 : public QObject , public WServerInterface
{
    Q_OBJECT
public:
    explicit DDEShellManagerInterfaceV1(QObject *parent = nullptr);
    ~DDEShellManagerInterfaceV1() override;

    void checkRegionalConflict(const QRegion &region);

Q_SIGNALS:
    void surfaceCreated(DDEShellSurfaceInterface *interface);
    void activeCreated(DDEActiveInterface *interface);
    void windowOverlapCheckerCreated(WindowOverlapCheckerInterface *interface);
    void multiTaskViewsCreated(MultiTaskViewInterface *interface);
    void PickerCreated(WindowPickerInterface *interface);
    void lockScreenCreated(LockScreenInterface *interface);

    void toggleMultitaskview();
    void requestPickWindow(WindowPickerInterface *picker);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<DDEShellManagerInterfaceV1Private> d;
};

class DDEShellSurfaceInterfacePrivate;
class DDEShellSurfaceInterface : public QObject
{
    Q_OBJECT
public:
    enum Role {
        OVERLAY,
    };

    ~DDEShellSurfaceInterface() override;

    WSurface *wSurface() const;
    bool ddeShellSurfaceIsMappedToWsurface(const WSurface *surface);
    std::optional<QPoint> surfacePos() const;
    std::optional<DDEShellSurfaceInterface::Role> role() const;
    std::optional<uint32_t> yOffset() const;
    std::optional<bool> skipSwitcher() const;
    std::optional<bool> skipDockPreView() const;
    std::optional<bool> skipMutiTaskView() const;

    static DDEShellSurfaceInterface *get(wl_resource *native);
    static DDEShellSurfaceInterface *get(WSurface *surface);

Q_SIGNALS:
    void positionChanged(QPoint pos);
    void roleChanged(DDEShellSurfaceInterface::Role role);
    void yOffsetChanged(uint32_t offset);
    void skipSwitcherChanged(bool skip);
    void skipDockPreViewChanged(bool skip);
    void skipMutiTaskViewChanged(bool skip);

private:
    explicit DDEShellSurfaceInterface(wl_resource *surface, wl_resource *resource);

private:
    friend class DDEShellManagerInterfaceV1Private;
    std::unique_ptr<DDEShellSurfaceInterfacePrivate> d;
};

class DDEActiveInterfacePrivate;
class DDEActiveInterface : public QObject
{
    Q_OBJECT
public:
    enum ActiveReason {
        Mouse = 0,
        Wheel = 1,
    };

    ~DDEActiveInterface() override;

    WSeat *seat() const;

    void sendActiveIn(uint32_t reason);
    void sendActiveOut(uint32_t reason);
    void sendStartDrag();
    void sendDrop();

    static void sendActiveIn(uint32_t reason, const WSeat *seat);
    static void sendActiveOut(uint32_t reason, const WSeat *seat);
    static void sendStartDrag(const WSeat *seat);
    static void sendDrop(const WSeat *seat);

private:
    explicit DDEActiveInterface(wl_resource *seat, wl_resource *resource);

private:
    friend class DDEShellManagerInterfaceV1Private;
    std::unique_ptr<DDEActiveInterfacePrivate> d;
};

class WindowOverlapCheckerInterfacePrivate;
class WindowOverlapCheckerInterface : public QObject
{
    Q_OBJECT
public:
    enum Anchor
    {
        TOP = 1,
        RIGHT = 2,
        BOTTOM = 4,
        LEFT = 8,
    };
    Q_ENUM(Anchor)

    ~WindowOverlapCheckerInterface() override;
    void sendOverlapped(bool overlapped);

    static void checkRegionalConflict(const QRegion &region);

Q_SIGNALS:
    void refresh();

private:
    explicit WindowOverlapCheckerInterface(wl_resource *resource);

private:
    friend class DDEShellManagerInterfaceV1Private;
    std::unique_ptr<WindowOverlapCheckerInterfacePrivate> d;
};

class MultiTaskViewInterfacePrivate;
class MultiTaskViewInterface : public QObject
{
    Q_OBJECT
public:
    ~MultiTaskViewInterface() override;

Q_SIGNALS:
    void toggle();

private:
    explicit MultiTaskViewInterface(wl_resource *resource);

private:
    friend class DDEShellManagerInterfaceV1Private;
    std::unique_ptr<MultiTaskViewInterfacePrivate> d;
};

class WindowPickerInterfacePrivate;
class WindowPickerInterface : public QObject
{
    Q_OBJECT
public:
    ~WindowPickerInterface() override;
    void sendWindowPid(qint32 pid);

Q_SIGNALS:
    void pick(const QString &hint);
    void beforeDestroy();

private:
    explicit WindowPickerInterface(wl_resource *resource);

private:
    friend class DDEShellManagerInterfaceV1Private;
    std::unique_ptr<WindowPickerInterfacePrivate> d;
};

class LockScreenInterfacePrivate;
class LockScreenInterface : public QObject
{
    Q_OBJECT
public:
    ~LockScreenInterface() override;
Q_SIGNALS:
    void lock();
    void shutdown();
    void switchUser();

private:
    explicit LockScreenInterface(wl_resource *resource);

private:
    friend class DDEShellManagerInterfaceV1Private;
    std::unique_ptr<LockScreenInterfacePrivate> d;
};

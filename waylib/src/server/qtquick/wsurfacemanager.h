/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <wglobal.h>
#include <WSurfaceLayout>

QT_BEGIN_NAMESPACE
class QQuickWindow;
class QQuickRenderControl;
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WServer;
class WBackend;
class WOutput;
class WOutputLayout;
class WSurface;
class WSurfaceItem;
class WSeat;
class WSurfaceManagerPrivate;
class WAYLIB_SERVER_EXPORT WSurfaceManager : public WSurfaceLayout
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSurfaceManager)

    Q_PROPERTY(WServer *server READ server CONSTANT)

public:
    explicit WSurfaceManager(WServer *server, WOutputLayout *layout, QObject *parent = nullptr);

    enum class Layer {
        BackgroundBelow,
        Background,
        BackgroundAbove,

        WindowBelow,
        Window,
        WindowAbove,

        OverrlayBelow,
        Overlay,
        OverlayAbove
    };
    Q_ENUM(Layer)

    WServer *server() const;
    void initialize();

    void add(WSurface *surface) override;
    void remove(WSurface *surface) override;

protected Q_SLOTS:
    bool setOutput(WSurface *surface, WOutput *output) override;

    void map(WSurface *surface) override;
    void unmap(WSurface *surface) override;
    bool isMapped(WSurface *surface) const override;

    void requestMaximize(WSurface *surface) override;
    void requestUnmaximize(WSurface *surface) override;

protected:
    bool event(QEvent *event) override;

    virtual void handleOutputAdded(WOutput *output);
    virtual void handleOutputRemoved(WOutput *output);

    virtual QQuickWindow *createWindow(QQuickRenderControl *rc, WOutput *output);
    virtual QQuickItem *createSurfaceItem(WSurface *surface);
    virtual Layer surfaceLayer(WSurface *surface) const;
    virtual QQuickItem *layerItem(QQuickWindow *window, Layer layer) const;
};

WAYLIB_SERVER_END_NAMESPACE

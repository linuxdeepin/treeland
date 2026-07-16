// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wqmlcreator.h>

#include <QObject>
#include <QQmlEngine>
#include <QString>

// Minimal glass-effect configuration exposed to QML so that the upstream
// Blur.qml / RoundBlur.qml — which read `Helper.config.*` — work inside the
// standalone demo without the full treeland DConfig stack.
class GlassConfig : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Global Liquid Glass vs legacy MultiEffect dispatch
    Q_PROPERTY(bool glassEnabled READ glassEnabled WRITE setGlassEnabled NOTIFY glassEnabledChanged)
    // Blur tuning
    Q_PROPERTY(int blurStrength READ blurStrength WRITE setBlurStrength NOTIFY blurStrengthChanged)
    Q_PROPERTY(qreal blurAmount READ blurAmount WRITE setBlurAmount NOTIFY blurAmountChanged)
    Q_PROPERTY(qreal blurMultiplier READ blurMultiplier WRITE setBlurMultiplier NOTIFY blurMultiplierChanged)
    // Glass material parameters
    Q_PROPERTY(qreal glassBezel READ glassBezel WRITE setGlassBezel NOTIFY glassBezelChanged)
    Q_PROPERTY(qreal glassThickness READ glassThickness WRITE setGlassThickness NOTIFY glassThicknessChanged)
    Q_PROPERTY(qreal glassIor READ glassIor WRITE setGlassIor NOTIFY glassIorChanged)
    Q_PROPERTY(qreal glassSpecularOpacity READ glassSpecularOpacity WRITE setGlassSpecularOpacity NOTIFY glassSpecularOpacityChanged)
    Q_PROPERTY(qreal glassTintOpacity READ glassTintOpacity WRITE setGlassTintOpacity NOTIFY glassTintOpacityChanged)
    Q_PROPERTY(qreal glassShadow READ glassShadow WRITE setGlassShadow NOTIFY glassShadowChanged)

public:
    explicit GlassConfig(QObject *parent = nullptr);

    static GlassConfig *create(QQmlEngine *, QJSEngine *) { return new GlassConfig; }

    bool glassEnabled() const { return m_glassEnabled; }
    void setGlassEnabled(bool v) { if (m_glassEnabled != v) { m_glassEnabled = v; emit glassEnabledChanged(); } }
    int blurStrength() const { return m_blurStrength; }
    void setBlurStrength(int v) { if (m_blurStrength != v) { m_blurStrength = v; emit blurStrengthChanged(); } }
    qreal blurAmount() const { return m_blurAmount; }
    void setBlurAmount(qreal v) { if (m_blurAmount != v) { m_blurAmount = v; emit blurAmountChanged(); } }
    qreal blurMultiplier() const { return m_blurMultiplier; }
    void setBlurMultiplier(qreal v) { if (m_blurMultiplier != v) { m_blurMultiplier = v; emit blurMultiplierChanged(); } }
    qreal glassBezel() const { return m_glassBezel; }
    void setGlassBezel(qreal v) { if (m_glassBezel != v) { m_glassBezel = v; emit glassBezelChanged(); } }
    qreal glassThickness() const { return m_glassThickness; }
    void setGlassThickness(qreal v) { if (m_glassThickness != v) { m_glassThickness = v; emit glassThicknessChanged(); } }
    qreal glassIor() const { return m_glassIor; }
    void setGlassIor(qreal v) { if (m_glassIor != v) { m_glassIor = v; emit glassIorChanged(); } }
    qreal glassSpecularOpacity() const { return m_glassSpecularOpacity; }
    void setGlassSpecularOpacity(qreal v) { if (m_glassSpecularOpacity != v) { m_glassSpecularOpacity = v; emit glassSpecularOpacityChanged(); } }
    qreal glassTintOpacity() const { return m_glassTintOpacity; }
    void setGlassTintOpacity(qreal v) { if (m_glassTintOpacity != v) { m_glassTintOpacity = v; emit glassTintOpacityChanged(); } }
    qreal glassShadow() const { return m_glassShadow; }
    void setGlassShadow(qreal v) { if (m_glassShadow != v) { m_glassShadow = v; emit glassShadowChanged(); } }

Q_SIGNALS:
    void glassEnabledChanged();
    void blurStrengthChanged();
    void blurAmountChanged();
    void blurMultiplierChanged();
    void glassBezelChanged();
    void glassThicknessChanged();
    void glassIorChanged();
    void glassSpecularOpacityChanged();
    void glassTintOpacityChanged();
    void glassShadowChanged();

private:
    bool m_glassEnabled = true;
    int m_blurStrength = 20;
    qreal m_blurAmount = 1.0;
    qreal m_blurMultiplier = 0.0;
    qreal m_glassBezel = 60;
    qreal m_glassThickness = 50;
    qreal m_glassIor = 3.0;
    qreal m_glassSpecularOpacity = 0.55;
    qreal m_glassTintOpacity = 0.08;
    qreal m_glassShadow = 0.5;
};

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WOutputRenderWindow;
class WQuickOutputLayout;
class WCursor;
class WSeat;
class WBackend;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_renderer;
class qw_allocator;
class qw_compositor;
QW_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class Helper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WQmlCreator* outputCreator MEMBER m_outputCreator CONSTANT)
    Q_PROPERTY(QString wallpaperSource READ wallpaperSource WRITE setWallpaperSource NOTIFY wallpaperSourceChanged)
    Q_PROPERTY(GlassConfig* config READ config CONSTANT)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);

    void initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine);

    QString wallpaperSource() const;
    void setWallpaperSource(const QString &wallpaperSource);
    GlassConfig *config() const { return m_config; }
Q_SIGNALS:
    void wallpaperSourceChanged();

public:
    inline WBackend *backend() const {
        return m_backend;
    }

private:
    QString m_wallpaperSource;
    WServer *m_server = nullptr;
    WQmlCreator *m_outputCreator = nullptr;
    GlassConfig *m_config = new GlassConfig(this);

    WBackend *m_backend = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;
    qw_compositor *m_compositor = nullptr;
    WQuickOutputLayout *m_outputLayout = nullptr;
    WCursor *m_cursor = nullptr;
    QPointer<WSeat> m_seat;
};

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
    Q_PROPERTY(qreal glassSpecular READ glassSpecular WRITE setGlassSpecular NOTIFY glassSpecularChanged)
    Q_PROPERTY(qreal glassTint READ glassTint WRITE setGlassTint NOTIFY glassTintChanged)
    Q_PROPERTY(qreal glassContentEdgePull READ glassContentEdgePull WRITE setGlassContentEdgePull NOTIFY glassContentEdgePullChanged)
    Q_PROPERTY(qreal glassContentRampEnd READ glassContentRampEnd WRITE setGlassContentRampEnd NOTIFY glassContentRampEndChanged)
    Q_PROPERTY(qreal glassRefractionMaxTan READ glassRefractionMaxTan WRITE setGlassRefractionMaxTan NOTIFY glassRefractionMaxTanChanged)
    Q_PROPERTY(qreal glassBrightness READ glassBrightness WRITE setGlassBrightness NOTIFY glassBrightnessChanged)
    Q_PROPERTY(qreal glassContrast READ glassContrast WRITE setGlassContrast NOTIFY glassContrastChanged)
    Q_PROPERTY(qreal glassSaturation READ glassSaturation WRITE setGlassSaturation NOTIFY glassSaturationChanged)

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
    qreal glassSpecular() const { return m_glassSpecular; }
    void setGlassSpecular(qreal v) { if (m_glassSpecular != v) { m_glassSpecular = v; emit glassSpecularChanged(); } }
    qreal glassTint() const { return m_glassTint; }
    void setGlassTint(qreal v) { if (m_glassTint != v) { m_glassTint = v; emit glassTintChanged(); } }
    qreal glassContentEdgePull() const { return m_glassContentEdgePull; }
    void setGlassContentEdgePull(qreal v) { if (m_glassContentEdgePull != v) { m_glassContentEdgePull = v; emit glassContentEdgePullChanged(); } }
    qreal glassContentRampEnd() const { return m_glassContentRampEnd; }
    void setGlassContentRampEnd(qreal v) { if (m_glassContentRampEnd != v) { m_glassContentRampEnd = v; emit glassContentRampEndChanged(); } }
    qreal glassRefractionMaxTan() const { return m_glassRefractionMaxTan; }
    void setGlassRefractionMaxTan(qreal v) { if (m_glassRefractionMaxTan != v) { m_glassRefractionMaxTan = v; emit glassRefractionMaxTanChanged(); } }
    qreal glassBrightness() const { return m_glassBrightness; }
    void setGlassBrightness(qreal v) { if (m_glassBrightness != v) { m_glassBrightness = v; emit glassBrightnessChanged(); } }
    qreal glassContrast() const { return m_glassContrast; }
    void setGlassContrast(qreal v) { if (m_glassContrast != v) { m_glassContrast = v; emit glassContrastChanged(); } }
    qreal glassSaturation() const { return m_glassSaturation; }
    void setGlassSaturation(qreal v) { if (m_glassSaturation != v) { m_glassSaturation = v; emit glassSaturationChanged(); } }

Q_SIGNALS:
    void glassEnabledChanged();
    void blurStrengthChanged();
    void blurAmountChanged();
    void blurMultiplierChanged();
    void glassBezelChanged();
    void glassThicknessChanged();
    void glassIorChanged();
    void glassSpecularChanged();
    void glassTintChanged();
    void glassContentEdgePullChanged();
    void glassContentRampEndChanged();
    void glassRefractionMaxTanChanged();
    void glassBrightnessChanged();
    void glassContrastChanged();
    void glassSaturationChanged();

private:
    bool m_glassEnabled = true;
    int m_blurStrength = 20;
    qreal m_blurAmount = 0.6;
    qreal m_blurMultiplier = 0.0;
    qreal m_glassBezel = 60;
    qreal m_glassThickness = 50;
    qreal m_glassIor = 1.5;
    qreal m_glassSpecular = 0.0;
    qreal m_glassTint = 0.0;
    qreal m_glassContentEdgePull = 0.42;
    qreal m_glassContentRampEnd = 0.50;
    qreal m_glassRefractionMaxTan = 2.75;
    qreal m_glassBrightness = 0.0;
    qreal m_glassContrast = 0.0;
    qreal m_glassSaturation = 0.04;
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

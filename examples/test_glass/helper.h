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
    Q_PROPERTY(qreal glassDisplacementFactor READ glassDisplacementFactor WRITE setGlassDisplacementFactor NOTIFY glassDisplacementFactorChanged)
    Q_PROPERTY(qreal glassIor READ glassIor WRITE setGlassIor NOTIFY glassIorChanged)
    Q_PROPERTY(qreal glassDispersion READ glassDispersion WRITE setGlassDispersion NOTIFY glassDispersionChanged)
    Q_PROPERTY(qreal glassBrightness READ glassBrightness WRITE setGlassBrightness NOTIFY glassBrightnessChanged)
    Q_PROPERTY(qreal glassEdgeSaturation READ glassEdgeSaturation WRITE setGlassEdgeSaturation NOTIFY glassEdgeSaturationChanged)
    Q_PROPERTY(qreal glassLightAngle READ glassLightAngle WRITE setGlassLightAngle NOTIFY glassLightAngleChanged)
    Q_PROPERTY(qreal glassReflectionOffset READ glassReflectionOffset WRITE setGlassReflectionOffset NOTIFY glassReflectionOffsetChanged)
    Q_PROPERTY(bool glassHighlightEnabled READ glassHighlightEnabled WRITE setGlassHighlightEnabled NOTIFY glassHighlightEnabledChanged)

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
    qreal glassDisplacementFactor() const { return m_glassDisplacementFactor; }
    void setGlassDisplacementFactor(qreal v) { if (m_glassDisplacementFactor != v) { m_glassDisplacementFactor = v; emit glassDisplacementFactorChanged(); } }
    qreal glassIor() const { return m_glassIor; }
    void setGlassIor(qreal v) { if (m_glassIor != v) { m_glassIor = v; emit glassIorChanged(); } }
    qreal glassDispersion() const { return m_glassDispersion; }
    void setGlassDispersion(qreal v) { if (m_glassDispersion != v) { m_glassDispersion = v; emit glassDispersionChanged(); } }
    qreal glassBrightness() const { return m_glassBrightness; }
    void setGlassBrightness(qreal v) { if (m_glassBrightness != v) { m_glassBrightness = v; emit glassBrightnessChanged(); } }
    qreal glassEdgeSaturation() const { return m_glassEdgeSaturation; }
    void setGlassEdgeSaturation(qreal v) { if (m_glassEdgeSaturation != v) { m_glassEdgeSaturation = v; emit glassEdgeSaturationChanged(); } }
    qreal glassLightAngle() const { return m_glassLightAngle; }
    void setGlassLightAngle(qreal v) { if (m_glassLightAngle != v) { m_glassLightAngle = v; emit glassLightAngleChanged(); } }
    qreal glassReflectionOffset() const { return m_glassReflectionOffset; }
    void setGlassReflectionOffset(qreal v) { if (m_glassReflectionOffset != v) { m_glassReflectionOffset = v; emit glassReflectionOffsetChanged(); } }
    bool glassHighlightEnabled() const { return m_glassHighlightEnabled; }
    void setGlassHighlightEnabled(bool v) { if (m_glassHighlightEnabled != v) { m_glassHighlightEnabled = v; emit glassHighlightEnabledChanged(); } }

Q_SIGNALS:
    void glassEnabledChanged();
    void blurStrengthChanged();
    void blurAmountChanged();
    void blurMultiplierChanged();
    void glassBezelChanged();
    void glassThicknessChanged();
    void glassDisplacementFactorChanged();
    void glassIorChanged();
    void glassDispersionChanged();
    void glassBrightnessChanged();
    void glassEdgeSaturationChanged();
    void glassLightAngleChanged();
    void glassReflectionOffsetChanged();
    void glassHighlightEnabledChanged();

private:
    bool m_glassEnabled = true;
    int m_blurStrength = 20;
    qreal m_blurAmount = 1.0;
    qreal m_blurMultiplier = 0.0;
    qreal m_glassBezel = 32;
    qreal m_glassThickness = 72;
    qreal m_glassDisplacementFactor = 0.65;
    qreal m_glassIor = 1.4;
    qreal m_glassDispersion = 0.05;
    qreal m_glassBrightness = 0.0;
    qreal m_glassEdgeSaturation = 0.0;
    qreal m_glassLightAngle = -127;
    qreal m_glassReflectionOffset = 12.0;
    bool m_glassHighlightEnabled = false;
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

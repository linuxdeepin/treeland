// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "mpvvideocontroller.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <QThread>
#include <QQuickItem>
#include <QQuickFramebufferObject>
#include <QTimer>
#include <QElapsedTimer>

class MpvVideoItem;

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvVideoItem *item);
    ~MpvRenderer() = default;

    MpvVideoItem *m_item = nullptr;
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void render() override;
};

class MpvVideoItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QString formattedPosition READ formattedPosition NOTIFY positionChanged)
    Q_PROPERTY(QString formattedDuration READ formattedDuration NOTIFY durationChanged)
    Q_PROPERTY(bool pause READ pause WRITE setPause NOTIFY pauseChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool loopFile READ loopFile WRITE setLoopFile NOTIFY loopFileChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool mute READ mute WRITE setMute NOTIFY muteChanged FINAL)
    Q_PROPERTY(VideoScaleMode scaleMode READ scaleMode WRITE setScaleMode NOTIFY scaleModeChanged FINAL)
    Q_PROPERTY(double panScan READ panScan WRITE setPanScan NOTIFY panScanChanged FINAL)
    Q_PROPERTY(int refreshInterval READ refreshInterval WRITE setRefreshInterval NOTIFY refreshIntervalChanged FINAL)
public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    enum class AsyncIds {
        None,
        SetVolume,
        GetVolume,
        ExpandText,
    };
    Q_ENUM(AsyncIds)

    enum Property {
        MediaTitle,
        Position,
        Duration,
        Pause,
        Volume,
        Mute,
        LoopFile,
        Speed,
        VideoUnscaled,
        PanScan
    };
    Q_ENUM(Property)

    enum VideoScaleMode {
        Scaled,
        Unscaled
    };
    Q_ENUM(VideoScaleMode)

    static QByteArrayView toByteArray(Property p);

    Renderer *createRenderer() const override;

    QString mediaTitle();

    double position();
    void setPosition(double value);

    double duration();

    QString formattedPosition();
    QString formattedDuration();

    bool pause();
    void setPause(bool value);

    int volume();
    void setVolume(int value);

    QString source();
    void setSource(const QString &source);

    bool loopFile();
    void setLoopFile(bool value);

    double speed();
    void setSpeed(double value);

    void setReady(bool ready);

    bool mute();
    void setMute(bool value);

    VideoScaleMode scaleMode();
    void setScaleMode(VideoScaleMode mode);

    double panScan();
    void setPanScan(double value);

    void slowDown(uint32_t duration);

    int refreshInterval() const;
    void setRefreshInterval(int value);

    Q_INVOKABLE void loadFile(const QString &file);

    Q_INVOKABLE int setPropertyBlocking(const QByteArrayView &property, const QVariant &value);
    Q_INVOKABLE void setPropertyAsync(const QByteArrayView &property, const QVariant &value, int id = 0);
    Q_INVOKABLE QVariant getProperty(const QByteArrayView &property);
    Q_INVOKABLE void getPropertyAsync(const QByteArrayView &property, int id = 0);
    Q_INVOKABLE QVariant commandBlocking(const QVariant &params);
    Q_INVOKABLE void commandAsync(const QStringList &params, int id = 0);
    Q_INVOKABLE QVariant expandText(const QString &text);
    Q_INVOKABLE int unobserveProperty(uint64_t id);

Q_SIGNALS:
    void mediaTitleChanged();
    void currentUrlChanged();
    void positionChanged();
    void durationChanged();
    void pauseChanged();
    void volumeChanged();
    void sourceChanged();
    void loopFileChanged();
    void speedChanged();
    void muteChanged();
    void scaleModeChanged();
    void panScanChanged();
    void refreshIntervalChanged();

    void fileStarted();
    void fileLoaded();
    void endFile(const QByteArray &reason);
    void videoReconfig();

    void ready();
    void observeProperty(const QByteArrayView &property, mpv_format format, uint64_t id = 0);
    void setProperty(const QByteArrayView &property, const QVariant &value);
    void command(const QStringList &params);

private Q_SLOTS:
    void onPropertyChanged(const QByteArrayView &property, const QVariant &value);
    void onAsyncReply(const QVariant &data, mpv_event event);
    void updatePlaybackSpeed();

private:
   void initConnections();
   QString formatTime(const double time);

private:
    friend class MpvRenderer;

    QThread *m_workerThread = nullptr;
    MpvVideoController *m_mpvController = nullptr;
    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_mpvGL = nullptr;

    QTimer *m_speedTimer = nullptr;
    QElapsedTimer m_elapsed;

    QUrl m_file;
    QString m_source;
    bool m_readyed = false;
    int m_refreshInterval = 16;
    uint32_t m_slowDownDuration;
    bool m_pause = false;
};

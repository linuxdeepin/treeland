// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "mpvvideoitem.h"
#include "loggings.h"

#include <QQuickWindow>
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>
#include <QEasingCurve>

static void *getGLProcAddress(void *ctx, const char *name)
{
    Q_UNUSED(ctx)

    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx) {
        return nullptr;
    }

    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}

static void handleMpvRedraw(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<MpvVideoItem *>(ctx),
                              &MpvVideoItem::update,
                              Qt::QueuedConnection);
}

MpvRenderer::MpvRenderer(MpvVideoItem *item)
    : m_item(item)
{
    m_item->window()->setPersistentSceneGraph(true);
}

QOpenGLFramebufferObject *MpvRenderer::createFramebufferObject(const QSize &size)
{
    if (!m_item->m_mpvGL) {
#if MPV_CLIENT_API_VERSION < MPV_MAKE_VERSION(2, 0)
        mpv_opengl_init_params gl_init_params{getGLProcAddress, nullptr, nullptr};
#else
        mpv_opengl_init_params gl_init_params{getGLProcAddress, nullptr};
#endif
        mpv_render_param display{MPV_RENDER_PARAM_INVALID, nullptr};
        if (qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
            display.type = MPV_RENDER_PARAM_X11_DISPLAY;
            display.data = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->display();
        }

        if (qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
            display.type = MPV_RENDER_PARAM_WL_DISPLAY;
            display.data = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()->display();
        }
        mpv_render_param params[]{{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                                   {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
                                   display,
                                   {MPV_RENDER_PARAM_INVALID, nullptr}};

        int result = mpv_render_context_create(&m_item->m_mpvGL, m_item->m_mpv, params);
        if (result < 0) {
            qCCritical(WALLPAPER) << "failed to initialize mpv GL context";
        } else {
            mpv_render_context_set_update_callback(m_item->m_mpvGL, handleMpvRedraw, m_item);
            Q_EMIT m_item->ready();
            m_item->setReady(true);
        }
    }

    return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
}

void MpvRenderer::render()
{
    QOpenGLFramebufferObject *fbo = framebufferObject();
    mpv_opengl_fbo mpfbo;
    mpfbo.fbo = static_cast<int>(fbo->handle());
    mpfbo.w = fbo->width();
    mpfbo.h = fbo->height();
    mpfbo.internal_format = 0;

    int flip_y{0};

    mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
                                  {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
                                  {MPV_RENDER_PARAM_INVALID, nullptr}};
    mpv_render_context_render(m_item->m_mpvGL, params);
}

MpvVideoItem::MpvVideoItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    if (QQuickWindow::graphicsApi() != QSGRendererInterface::OpenGL) {
        qCCritical(WALLPAPER) << "error, The graphics api must be set to opengl or mpv won't be able to render the video.";
    }

    m_workerThread = new QThread;
    m_mpvController = new MpvVideoController;
    m_workerThread->start();
    m_mpvController->moveToThread(m_workerThread);
    QMetaObject::invokeMethod(m_mpvController,
                              &MpvVideoController::init,
                              Qt::BlockingQueuedConnection);

    m_mpv = m_mpvController->mpv();

    connect(m_workerThread,
            &QThread::finished,
            m_mpvController,
            &MpvVideoController::deleteLater);
    connect(this,
            &MpvVideoItem::observeProperty,
            m_mpvController,
            &MpvVideoController::observeProperty,
            Qt::QueuedConnection);
    connect(this,
            &MpvVideoItem::setProperty,
            m_mpvController,
            &MpvVideoController::setProperty,
            Qt::QueuedConnection);
    connect(this,
            &MpvVideoItem::command,
            m_mpvController,
            &MpvVideoController::command,
            Qt::QueuedConnection);

    observeProperty(MpvVideoItem::toByteArray(MediaTitle), MPV_FORMAT_STRING);
    observeProperty(MpvVideoItem::toByteArray(Position), MPV_FORMAT_DOUBLE);
    observeProperty(MpvVideoItem::toByteArray(Duration), MPV_FORMAT_DOUBLE);
    observeProperty(MpvVideoItem::toByteArray(Pause), MPV_FORMAT_FLAG);
    observeProperty(MpvVideoItem::toByteArray(Volume), MPV_FORMAT_INT64);
    observeProperty(MpvVideoItem::toByteArray(LoopFile), MPV_FORMAT_STRING);
    observeProperty(MpvVideoItem::toByteArray(Speed), MPV_FORMAT_DOUBLE);
    observeProperty(MpvVideoItem::toByteArray(VideoUnscaled), MPV_FORMAT_STRING);
    observeProperty(MpvVideoItem::toByteArray(PanScan), MPV_FORMAT_DOUBLE);

    initConnections();

    setPropertyAsync(MpvVideoItem::toByteArray(Volume), 0, static_cast<int>(AsyncIds::SetVolume));
    setMute(true);
    getPropertyAsync(MpvVideoItem::toByteArray(Volume), static_cast<int>(AsyncIds::GetVolume));
    setLoopFile(true);
    setScaleMode(Scaled);
    setPanScan(1.0);
}

MpvVideoItem::~MpvVideoItem()
{
    if (m_mpvGL) {
        mpv_render_context_free(m_mpvGL);
    }
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);

    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
    }

    mpv_terminate_destroy(m_mpv);
}

QByteArrayView MpvVideoItem::toByteArray(Property p)
{
    switch (p) {
    case MediaTitle:  return "media-title";
    case Position:    return "time-pos";
    case Duration:    return "duration";
    case Pause:       return "pause";
    case Volume:      return "volume";
    case Mute:        return "mute";
    case LoopFile:    return "loop-file";
    case Speed:       return "speed";
    case VideoUnscaled: return "video-unscaled";
    case PanScan:     return "panscan";
    }
    return "";
}

QString MpvVideoItem::mediaTitle()
{
    return getProperty(MpvVideoItem::toByteArray(MediaTitle)).toString();
}

double MpvVideoItem::position()
{
    return getProperty(MpvVideoItem::toByteArray(Position)).toDouble();
}

void MpvVideoItem::setPosition(double value)
{
    if (qFuzzyCompare(value, position())) {
        return;
    }

    Q_EMIT setPropertyAsync(MpvVideoItem::toByteArray(Position), value);
}

double MpvVideoItem::duration()
{
    return getProperty(MpvVideoItem::toByteArray(Duration)).toDouble();
}

QString MpvVideoItem::formattedPosition()
{
    return formatTime(position());
}

QString MpvVideoItem::formattedDuration()
{
    return formatTime(duration());
}

bool MpvVideoItem::pause()
{
    return m_pause;
}

void MpvVideoItem::setPause(bool value)
{
    if (m_pause == value) {
        return;
    }
    m_pause = value;

    if (m_speedTimer) {
        m_speedTimer->stop();
        delete m_speedTimer;
        m_speedTimer = nullptr;
    }

    setSpeed(1.0);
    Q_EMIT setPropertyAsync(MpvVideoItem::toByteArray(Pause), value);
}

int MpvVideoItem::volume()
{
    return getProperty(MpvVideoItem::toByteArray(Volume)).toInt();
}

void MpvVideoItem::setVolume(int value)
{
    if (value == volume()) {
        return;
    }
    Q_EMIT setPropertyAsync(MpvVideoItem::toByteArray(Volume), value);
}

QString MpvVideoItem::source()
{
    return m_source;
}

void MpvVideoItem::setSource(const QString &source)
{
    if (m_source == source) {
        return;
    }

    m_source = source;
    if (m_readyed) {
        loadFile(source);
    } else {
        connect(this, &MpvVideoItem::ready, this, [this]{ loadFile(m_source); });
    }
    Q_EMIT sourceChanged();
}

bool MpvVideoItem::loopFile()
{
    if (getProperty(MpvVideoItem::toByteArray(LoopFile)).toString() == "inf")
        return true;

    return false;
}

void MpvVideoItem::setLoopFile(bool value)
{
    if (loopFile() == value) {
        return;
    }

    if (value) {
        Q_EMIT setPropertyAsync(MpvVideoItem::toByteArray(LoopFile), "inf");
    } else {
        Q_EMIT setPropertyAsync(MpvVideoItem::toByteArray(LoopFile), "no");
    }
}


double MpvVideoItem::speed()
{
    return getProperty(MpvVideoItem::toByteArray(Speed)).toDouble();
}

void MpvVideoItem::setSpeed(double value)
{
    if (value == speed()) {
        return;
    }

    Q_EMIT setPropertyAsync(MpvVideoItem::toByteArray(Speed), value);
}

void MpvVideoItem::setReady(bool ready)
{
    if (m_readyed == ready) {
        return;
    }

    m_readyed = ready;
}

bool MpvVideoItem::mute()
{
    return getProperty(MpvVideoItem::toByteArray(Mute)).toBool();
}

void MpvVideoItem::setMute(bool value)
{
    if (value == mute()) {
        return;
    }

    Q_EMIT setPropertyAsync(MpvVideoItem::toByteArray(Mute), value);
}

inline static const char *toMpvVideoUnscaled(MpvVideoItem::VideoScaleMode mode)
{
    return mode == MpvVideoItem::Scaled ? "no" : "yes";
}

inline static MpvVideoItem::VideoScaleMode fromMpvVideoUnscaled(const QString &value)
{
    if (value == QLatin1String("yes") || value == QLatin1String("true")
        || value == QLatin1String("1")) {
        return MpvVideoItem::Unscaled;
    }

    return MpvVideoItem::Scaled;
}

MpvVideoItem::VideoScaleMode MpvVideoItem::scaleMode()
{
    const QString value =
        getProperty(MpvVideoItem::toByteArray(VideoUnscaled)).toString();

    return fromMpvVideoUnscaled(value);
}

void MpvVideoItem::setScaleMode(VideoScaleMode mode)
{
    if (mode == scaleMode())
        return;

    Q_EMIT setPropertyAsync(
        MpvVideoItem::toByteArray(VideoUnscaled),
        QLatin1String(toMpvVideoUnscaled(mode))
        );
}

double MpvVideoItem::panScan()
{
    return getProperty(MpvVideoItem::toByteArray(PanScan)).toDouble();
}

void MpvVideoItem::setPanScan(double value)
{
    if (value == panScan()) {
        return;
    }

    Q_EMIT setPropertyAsync(MpvVideoItem::toByteArray(PanScan), value);
}

void MpvVideoItem::slowDown(uint32_t duration)
{
    if (m_speedTimer) {
        return;
    }

    m_speedTimer = new QTimer(this);
    m_speedTimer->setInterval(refreshInterval());
    m_slowDownDuration = duration;
    connect(m_speedTimer, &QTimer::timeout, this, &MpvVideoItem::updatePlaybackSpeed);

    m_elapsed.restart();
    m_speedTimer->start();
}

int MpvVideoItem::refreshInterval() const
{
    return m_refreshInterval;
}

void MpvVideoItem::setRefreshInterval(int value)
{
    if (m_refreshInterval == value) {
        return;
    }

    m_refreshInterval = value;
    Q_EMIT refreshIntervalChanged();
}

void MpvVideoItem::loadFile(const QString &file)
{
    auto url = QUrl::fromUserInput(file);
    if (m_file != url) {
        m_file = url;
        Q_EMIT currentUrlChanged();
    }

    Q_EMIT command(QStringList() << QStringLiteral("loadfile")
                                 << m_file.toString(QUrl::PreferLocalFile));
}

int MpvVideoItem::setPropertyBlocking(const QByteArrayView &property, const QVariant &value)
{
    int error;
    QMetaObject::invokeMethod(m_mpvController,
                              "setProperty",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, error),
                              Q_ARG(QByteArrayView, property),
                              Q_ARG(QVariant, value));

    return error;
}

void MpvVideoItem::setPropertyAsync(const QByteArrayView &property,
                                    const QVariant &value, int id)
{

    QMetaObject::invokeMethod(m_mpvController,
                              "setPropertyAsync",
                              Qt::QueuedConnection,
                              Q_ARG(QByteArrayView, property),
                              Q_ARG(QVariant, value),
                              Q_ARG(int, id));
}

QVariant MpvVideoItem::getProperty(const QByteArrayView &property)
{
    QVariant value;
    QMetaObject::invokeMethod(m_mpvController,
                              "getProperty",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVariant, value),
                              Q_ARG(QByteArrayView, property));

    return value;
}

void MpvVideoItem::getPropertyAsync(const QByteArrayView &property, int id)
{
    QMetaObject::invokeMethod(m_mpvController,
                              "getPropertyAsync",
                              Qt::QueuedConnection,
                              Q_ARG(QByteArrayView, property),
                              Q_ARG(int, id));
}

QVariant MpvVideoItem::commandBlocking(const QVariant &params)
{
    QVariant value;
    QMetaObject::invokeMethod(m_mpvController,
                              "command",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVariant, value),
                              Q_ARG(QVariant, params));

    return value;
}

void MpvVideoItem::commandAsync(const QStringList &params, int id)
{
    QMetaObject::invokeMethod(m_mpvController,
                              "commandAsync",
                              Qt::QueuedConnection,
                              Q_ARG(QVariant, params),
                              Q_ARG(int, id));
}

QVariant MpvVideoItem::expandText(const QString &text)
{
    QVariant value;
    QMetaObject::invokeMethod(m_mpvController,
                              "command",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVariant, value),
                              Q_ARG(QVariant, QVariant::fromValue(QStringList{QStringLiteral("expand-text"), text})));

    return value;
}

int MpvVideoItem::unobserveProperty(uint64_t id)
{
    return m_mpvController->unobserveProperty(id);
}

void MpvVideoItem::onPropertyChanged(const QByteArrayView &property,
                                     [[maybe_unused]] const QVariant &value)
{
    if (property == toByteArray(MediaTitle)) {
        Q_EMIT mediaTitleChanged();
    } else if (property == toByteArray(Position)) {
        Q_EMIT positionChanged();
    } else if (property == toByteArray(Duration)) {
        Q_EMIT durationChanged();
    } else if (property == toByteArray(Pause)) {
        Q_EMIT pauseChanged();
    } else if (property == toByteArray(Volume)) {
        Q_EMIT volumeChanged();
    } else if (property == toByteArray(LoopFile)) {
        Q_EMIT loopFileChanged();
    } else if (property == toByteArray(Speed)) {
        Q_EMIT speedChanged();
    } else if (property == toByteArray(Mute)) {
        Q_EMIT muteChanged();
    }
}

void MpvVideoItem::onAsyncReply(const QVariant &data, mpv_event event)
{
    switch (static_cast<AsyncIds>(event.reply_userdata)) {
    case AsyncIds::None: {
        break;
    }
    case AsyncIds::SetVolume: {
        qCDebug(WALLPAPER) << "onSetPropertyReply"
                           << event.reply_userdata;
        break;
    }
    case AsyncIds::GetVolume: {
        qCDebug(WALLPAPER) << "onGetPropertyReply"
                           << event.reply_userdata
                           << data;
        break;
    }
    case AsyncIds::ExpandText: {
        qCDebug(WALLPAPER) << "onGetPropertyReply"
                           << event.reply_userdata
                           << data;
        break;
    }
    }
}

static inline double easeOutExpo(double t)
{
    static const QEasingCurve curve(QEasingCurve::OutExpo);
    return curve.valueForProgress(t);
}

void MpvVideoItem::updatePlaybackSpeed()
{
    constexpr double minSpeed = 0.0;
    constexpr double maxSpeed = 1.0;

    if (m_elapsed.hasExpired(m_slowDownDuration)) {
        m_speedTimer->stop();
        setPause(true);
        return;
    }

    double t = double(m_elapsed.elapsed()) / m_slowDownDuration;
    t = std::clamp(t, 0.0, 1.0);

    double ease = easeOutExpo(t);
    double speed = maxSpeed - ease * (maxSpeed - minSpeed);
    setSpeed(speed);
}

void MpvVideoItem::initConnections()
{
    connect(m_mpvController, &MpvVideoController::propertyChanged,
            this, &MpvVideoItem::onPropertyChanged, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::fileStarted,
            this, &MpvVideoItem::fileStarted, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::fileLoaded,
            this, &MpvVideoItem::fileLoaded, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::endFile,
            this, &MpvVideoItem::endFile, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::videoReconfig,
            this, &MpvVideoItem::videoReconfig, Qt::QueuedConnection);
    connect(m_mpvController, &MpvVideoController::asyncReply,
            this, &MpvVideoItem::onAsyncReply, Qt::QueuedConnection);
}

QString MpvVideoItem::formatTime(const double time)
{
    int totalNumberOfSeconds = static_cast<int>(time);
    int seconds = totalNumberOfSeconds % 60;
    int minutes = (totalNumberOfSeconds / 60) % 60;
    int hours = (totalNumberOfSeconds / 60 / 60);

    QString timeString =
        QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));

    return timeString;
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const
{
    return new MpvRenderer(const_cast<MpvVideoItem *>(this));
}

// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fpsdisplaymanager.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QLoggingCategory>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QScreen>
#include <QFontDatabase>

Q_LOGGING_CATEGORY(qLcFpsDisplay, "treeland.fpsdisplay")

FpsDisplayManager::FpsDisplayManager(QObject *parent)
    : QObject(parent)
    , m_updateTimer(this)
    , m_vsyncTimer(this)
{
    m_timer.start();

    m_updateTimer.setInterval(UPDATE_INTERVAL_MS);
    m_updateTimer.setSingleShot(false);
    connect(&m_updateTimer, &QTimer::timeout, this, &FpsDisplayManager::updateFps);

    detectDisplayRefreshRate();

    double preciseInterval = 1000.0 / m_displayRefreshRate;
    int vsyncInterval = qRound(preciseInterval);
    m_preciseVSyncInterval = preciseInterval;

    // Optimize timer intervals for common refresh rates
    if (m_displayRefreshRate == 60) {
        vsyncInterval = 17; // 17ms for QTimer (closest to 16.67ms)
        m_preciseVSyncInterval = 16.666667; // Exact 60Hz interval (1000/60)
    } else if (m_displayRefreshRate == 90) {
        vsyncInterval = 11; // 11ms for QTimer (closest to 11.11ms)
        m_preciseVSyncInterval = 11.111111; // Exact 90Hz interval (1000/90)
    } else if (m_displayRefreshRate == 120) {
        vsyncInterval = 8; // 8ms for QTimer (closest to 8.33ms)
        m_preciseVSyncInterval = 8.333333; // Exact 120Hz interval (1000/120)
    } else if (m_displayRefreshRate == 144) {
        vsyncInterval = 7; // 7ms for QTimer (closest to 6.94ms)
        m_preciseVSyncInterval = 6.944444; // Exact 144Hz interval (1000/144)
    }

    m_vsyncTimer.setInterval(vsyncInterval);
    m_vsyncTimer.setSingleShot(false);
    connect(&m_vsyncTimer, &QTimer::timeout, this, &FpsDisplayManager::onVSyncTimer);
}

FpsDisplayManager::~FpsDisplayManager()
{
    destroyDisplayItem();
}

void FpsDisplayManager::setTargetWindow(QQuickWindow *window)
{
    if (m_targetWindow == window)
        return;

    destroyDisplayItem();
    m_targetWindow = window;

    if (m_targetWindow) {
        detectDisplayRefreshRate();
        double preciseInterval = 1000.0 / m_displayRefreshRate;
        int vsyncInterval = qRound(preciseInterval);
        m_preciseVSyncInterval = preciseInterval;

        // Optimize timer intervals for common refresh rates
        if (m_displayRefreshRate == 60) {
            vsyncInterval = 17; // 17ms for QTimer (closest to 16.67ms)
            m_preciseVSyncInterval = 16.666667; // Exact 60Hz interval (1000/60)
        } else if (m_displayRefreshRate == 90) {
            vsyncInterval = 11; // 11ms for QTimer (closest to 11.11ms)
            m_preciseVSyncInterval = 11.111111; // Exact 90Hz interval (1000/90)
        } else if (m_displayRefreshRate == 120) {
            vsyncInterval = 8; // 8ms for QTimer (closest to 8.33ms)
            m_preciseVSyncInterval = 8.333333; // Exact 120Hz interval (1000/120)
        } else if (m_displayRefreshRate == 144) {
            vsyncInterval = 7; // 7ms for QTimer (closest to 6.94ms)
            m_preciseVSyncInterval = 6.944444; // Exact 144Hz interval (1000/144)
        }

        m_vsyncTimer.setInterval(vsyncInterval);
    }

    if (m_visible && m_targetWindow) {
        createDisplayItem();
    }
}

void FpsDisplayManager::show()
{
    if (m_visible && m_displayItem)
        return;

    m_visible = true;

    if (m_targetWindow && !m_displayItem) {
        createDisplayItem();
        m_updateTimer.start();
        m_vsyncTimer.start();
        reset();
    }
}

void FpsDisplayManager::hide()
{
    if (!m_visible)
        return;

    m_visible = false;
    m_updateTimer.stop();
    m_vsyncTimer.stop();
    destroyDisplayItem();
}

void FpsDisplayManager::toggle()
{
    if (m_visible) {
        hide();
    } else {
        show();
    }
}

void FpsDisplayManager::onVSyncTimer()
{
    if (!m_visible)
        return;

    qint64 currentTime = m_timer.elapsed();

    if (m_lastVSyncTime_precise > 0) {
        qint64 actualInterval = currentTime - m_lastVSyncTime_precise;
        double expectedInterval = m_preciseVSyncInterval;

        if (actualInterval > expectedInterval * 1.5 || actualInterval < expectedInterval * 0.5) { // 50% tolerance for timer precision
            m_lastVSyncTime_precise = currentTime;
            return;
        }
    }

    m_vSyncTimes.enqueue(currentTime);
    m_lastVSyncTime_precise = currentTime;

    while (m_vSyncTimes.size() > MAX_SAMPLES) { // Keep last 120 samples (2 seconds at 60Hz)
        m_vSyncTimes.dequeue();
    }
}

void FpsDisplayManager::reset()
{
    m_currentFps = m_displayRefreshRate;
    m_averageFps = m_displayRefreshRate;
    m_maximumFps = m_displayRefreshRate;
    m_lastUpdateTime = m_timer.elapsed();
    m_vSyncTimes.clear();
    m_lastVSyncTime_precise = 0;

    if (m_displayItem) {
        updateFpsText();
    }
}

void FpsDisplayManager::updateFps()
{
    if (!m_visible || m_vSyncTimes.isEmpty())
        return;

    qint64 currentTime = m_timer.elapsed();
    qint64 timeDiff = currentTime - m_lastUpdateTime;

    if (timeDiff < UPDATE_INTERVAL_MS) // Update UI every 500ms for responsiveness
        return;

    qreal currentFps = 0.0;
    qreal averageFps = 0.0;

    if (m_vSyncTimes.size() >= 2) {
        qint64 totalTimeSpan = m_vSyncTimes.last() - m_vSyncTimes.first();
        if (totalTimeSpan > 0) {
            averageFps = ((m_vSyncTimes.size() - 1) * 1000.0) / totalTimeSpan;
        }

        if (m_vSyncTimes.size() >= 3) { // Need minimum 3 samples for instantaneous FPS
            qint64 recentTimeSpan = m_vSyncTimes.last() - m_vSyncTimes[m_vSyncTimes.size() - 3];
            if (recentTimeSpan > 0) {
                currentFps = (2 * 1000.0) / recentTimeSpan;
            }
        } else {
            currentFps = averageFps;
        }
    } else if (m_vSyncTimes.size() == 1) {
        currentFps = averageFps = m_displayRefreshRate;
    }

    if (currentFps > 0 || averageFps > 0) {
        const qreal smoothingFactor = 0.3; // 30% new value, 70% previous for smooth transitions
        if (m_currentFps == 0.0) {
            m_currentFps = currentFps;
            m_averageFps = averageFps;
        } else {
            m_currentFps = m_currentFps * (1.0 - smoothingFactor) + currentFps * smoothingFactor;
            m_averageFps = m_averageFps * (1.0 - smoothingFactor) + averageFps * smoothingFactor;
        }

        double maxFps = m_displayRefreshRate + 1.0; // Allow 1 FPS tolerance above refresh rate
        m_currentFps = qBound(1.0, m_currentFps, maxFps);
        m_averageFps = qBound(1.0, m_averageFps, maxFps);

        if (m_currentFps > m_maximumFps) {
            m_maximumFps = m_currentFps;
        }
    }

    m_lastUpdateTime = currentTime;
    updateFpsText();
}

void FpsDisplayManager::detectDisplayRefreshRate()
{
    m_displayRefreshRate = 60; // Default 60Hz fallback for most displays
    if (m_targetWindow) {
        if (auto screen = m_targetWindow->screen()) {
            qreal refreshRate = screen->refreshRate();
            if (refreshRate > 0) {
                m_displayRefreshRate = qRound(refreshRate);
                return;
            }
        }
    }

    // Environment variable override for testing/debugging
    QString envRefreshRate = qgetenv("TREELAND_REFRESH_RATE");
    if (!envRefreshRate.isEmpty()) {
        bool ok;
        int rate = envRefreshRate.toInt(&ok);
        if (ok && rate > 0 && rate <= 240) { // Support up to 240Hz displays
            m_displayRefreshRate = rate;
        }
    }
}

class FpsDisplayItem : public QQuickPaintedItem
{
    Q_OBJECT
public:
    explicit FpsDisplayItem(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        setFlag(QQuickItem::ItemAcceptsInputMethod, false);
        setFlag(QQuickItem::ItemAcceptsDrops, false);
        setFlag(QQuickItem::ItemIsFocusScope, false);
        setFlag(QQuickItem::ItemHasContents, true);
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
        setAcceptTouchEvents(false);
        setActiveFocusOnTab(false);
        setFocus(false);
        setEnabled(false);
        setVisible(false);
        setWidth(130);
        setHeight(45);
    }

    void paint(QPainter *painter) override
    {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::TextAntialiasing);

        QFont font;
        qreal scaleFactor = 1.0;
        if (width() > 150) {
            scaleFactor = width() / 130.0;
        }
        int fontSize = qRound(14 * scaleFactor);

        font.setPixelSize(fontSize);
        font.setWeight(QFont::Normal);
        font.setHintingPreference(QFont::PreferFullHinting);
        painter->setFont(font);

        qreal lineHeight = height() / 2.2;
        qreal lineSpacing = height() / 2.0;

        painter->setPen(QColor(255, 255, 255, 150));
        painter->drawText(QRectF(1, 1, width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter, m_fpsText);
        painter->drawText(QRectF(1, lineSpacing + 1, width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter, m_avgFpsText);

        painter->setPen(QColor(0, 0, 0));
        painter->drawText(QRectF(0, 0, width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter, m_fpsText);
        painter->drawText(QRectF(0, lineSpacing, width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter, m_avgFpsText);
    }

    void setFpsText(const QString &text, const QColor &color)
    {
        if (m_fpsText != text || m_fpsColor != color) {
            m_fpsText = text;
            m_fpsColor = color;
            update();
        }
    }

    void setAvgFpsText(const QString &text)
    {
        if (m_avgFpsText != text) {
            m_avgFpsText = text;
            update();
        }
    }

protected:
    // Override all event handlers to ensure complete input isolation
    bool event(QEvent *event) override
    {
        // Only handle paint-related events, reject all input and focus events
        switch (event->type()) {
        case QEvent::Paint:
        case QEvent::UpdateRequest:
        case QEvent::Polish:
        case QEvent::PolishRequest:
            return QQuickPaintedItem::event(event);
        case QEvent::FocusIn:
        case QEvent::FocusOut:
        case QEvent::FocusAboutToChange:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
        case QEvent::Enter:
        case QEvent::Leave:
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::InputMethod:
        case QEvent::InputMethodQuery:
            event->ignore();
            return false;
        default:
            if (event->type() >= QEvent::User) {
                event->ignore();
                return false;
            }
            return QQuickPaintedItem::event(event);
        }
    }

    void mousePressEvent(QMouseEvent *event) override { event->ignore(); }
    void mouseReleaseEvent(QMouseEvent *event) override { event->ignore(); }
    void mouseMoveEvent(QMouseEvent *event) override { event->ignore(); }
    void keyPressEvent(QKeyEvent *event) override { event->ignore(); }
    void keyReleaseEvent(QKeyEvent *event) override { event->ignore(); }
    void focusInEvent(QFocusEvent *event) override { event->ignore(); }
    void focusOutEvent(QFocusEvent *event) override { event->ignore(); }
    void hoverEnterEvent(QHoverEvent *event) override { event->ignore(); }
    void hoverLeaveEvent(QHoverEvent *event) override { event->ignore(); }
    void hoverMoveEvent(QHoverEvent *event) override { event->ignore(); }
    void touchEvent(QTouchEvent *event) override { event->ignore(); }

private:
    QString m_fpsText = "Current FPS: 0";
    QString m_avgFpsText = "Maximum FPS: 0";
    QColor m_fpsColor = QColor(255, 255, 255);
};

void FpsDisplayManager::createDisplayItem()
{
    if (!m_targetWindow || m_displayItem)
        return;

    auto displayItem = new FpsDisplayItem(nullptr);
    displayItem->setParent(m_targetWindow);
    displayItem->setParentItem(m_targetWindow->contentItem());

    qreal windowWidth = m_targetWindow->contentItem()->width();

    qreal scaleFactor = 1.0;
    if (windowWidth > 2560) {
        scaleFactor = 1.5; // 4K+ displays need 50% larger UI
    } else if (windowWidth > 1920) {
        scaleFactor = 1.2; // 2K displays need 20% larger UI
    }

    qreal displayWidth = 130 * scaleFactor;
    qreal displayHeight = 45 * scaleFactor;
    qreal margin = 20 * scaleFactor;
    qreal topOffset = 40 * scaleFactor;

    displayItem->setWidth(displayWidth);
    displayItem->setHeight(displayHeight);
    displayItem->setX(windowWidth - displayWidth - margin);
    displayItem->setY(topOffset);
    displayItem->setZ(99999); // Ensure FPS display is always on top

    m_displayItem = displayItem;
    updateFpsText();

    QMetaObject::invokeMethod(this, [this]() {
        if (m_displayItem) {
            m_displayItem->setVisible(true);
            m_displayItem->setEnabled(false);
            m_displayItem->setFocus(false);
            m_displayItem->setActiveFocusOnTab(false);

            if (m_targetWindow && m_targetWindow->contentItem()) {
                auto contentItem = m_targetWindow->contentItem();
                if (!contentItem->hasActiveFocus() && !contentItem->hasFocus()) {
                    contentItem->setFocus(true);
                }
            }
        }
    }, Qt::QueuedConnection);
}

void FpsDisplayManager::destroyDisplayItem()
{
    if (m_displayItem) {
        m_displayItem->deleteLater();
        m_displayItem = nullptr;
    }
}

void FpsDisplayManager::updateFpsText()
{
    auto displayItem = qobject_cast<FpsDisplayItem*>(m_displayItem);
    if (!displayItem)
        return;

    QString currentFpsText = QString("Current FPS: %1").arg(qRound(m_currentFps));
    QString maximumFpsText = QString("Maximum FPS: %1").arg(qRound(m_maximumFps));
    QColor fpsColor = QColor(255, 255, 255);

    displayItem->setFpsText(currentFpsText, fpsColor);
    displayItem->setAvgFpsText(maximumFpsText);
}

#include "fpsdisplaymanager.moc"

// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickcursor.h"
#include "private/wcursor_p.h"
#include "woutputrenderwindow.h"
#include "wquickoutputlayout.h"
#include "woutputviewport.h"
#include "wxcursorimage.h"

#include <qwxcursormanager.h>

#include <QQmlComponent>
#include <QQuickImageProvider>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickCursorPrivate : public WCursorPrivate
{
public:
    WQuickCursorPrivate(WQuickCursor *qq);
    ~WQuickCursorPrivate() {
        delete xcursor_manager;
    }

    inline static WQuickCursorPrivate *get(WQuickCursor *qq) {
        return qq->d_func();
    }

    // TODO: Allow add render window in public interface, because
    // maybe a WOutputRenderWindow is no attach output.
    void updateRenderWindows();
    void updateCurrentRenderWindow();
    void setCurrentRenderWindow(WOutputRenderWindow *window);
    void onRenderWindowAdded(WOutputRenderWindow *window);
    void onRenderWindowRemoved(WOutputRenderWindow *window);
    void onCursorPositionChanged();

    inline QString imageProviderId() const {
        return QString::number(quintptr(this), 16);
    }
    void setCursorImageUrl(const QUrl &url);
    void updateXCursorManager();

    inline quint32 getCursorSize() const {
        return qMax(cursorSize.width(), cursorSize.height());
    }

    W_DECLARE_PUBLIC(WQuickCursor)
    bool componentComplete = true;
    QList<WOutputRenderWindow*> renderWindows;
    WOutputRenderWindow *currentRenderWindow = nullptr;
    QList<QQuickItem*> cursorItems;
    QString xcursorThemeName;
    QSize cursorSize = QSize(24, 24);

    QQmlComponent *delegate = nullptr;
};

class CursorImageProvider : public QQuickImageProvider
{
public:
    CursorImageProvider(WQuickCursor *cursor)
        : QQuickImageProvider(QQmlImageProviderBase::Image)
        , m_cursor(cursor)
    {

    }

    QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize) override {
        Q_UNUSED(id)

        auto cd = WQuickCursorPrivate::get(m_cursor);
        quint32 requestCursorSize = qMax(requestedSize.width(), requestedSize.height());
        quint32 realCursorSize = cd->getCursorSize();
        qreal scale = qreal(requestCursorSize) / realCursorSize;

        if (qFuzzyIsNull(scale))
            scale = 1.0;

        auto cursorImage = m_cursor->getCursorImage(scale);
        if (!cursorImage)
            return QImage();

        if (size)
            *size = cursorImage->image().size();

        return cursorImage->image();
    }

    WQuickCursor *m_cursor;
};

WQuickCursorPrivate::WQuickCursorPrivate(WQuickCursor *qq)
    : WCursorPrivate(qq)
{

}

void WQuickCursorPrivate::updateRenderWindows()
{
    QList<WOutputRenderWindow*> newList;

    W_Q(WQuickCursor);
    for (auto o : q->layout()->outputs()) {
        QObject::connect(o, SIGNAL(windowChanged(QQuickWindow*)), q, SLOT(updateRenderWindows()));

        auto renderWindow = qobject_cast<WOutputRenderWindow*>(o->window());
        if (!renderWindow)
            continue;
        if (newList.contains(renderWindow))
            continue;
        newList.append(renderWindow);

        if (!renderWindows.contains(renderWindow)) {
            renderWindows.append(renderWindow);
            onRenderWindowAdded(renderWindow);
        }
    }

    for (int i = 0; i < renderWindows.count(); ++i) {
        auto window = renderWindows.at(i);
        if (!newList.contains(window)) {
            renderWindows.removeAt(i);
            --i;
            onRenderWindowRemoved(window);
        }
    }

    updateCurrentRenderWindow();
}

void WQuickCursorPrivate::updateCurrentRenderWindow()
{
    if (renderWindows.isEmpty()) {
        setCurrentRenderWindow(nullptr);
        return;
    }

    W_Q(WQuickCursor);
    const QPoint pos = q->position().toPoint();
    if (currentRenderWindow && currentRenderWindow->geometry().contains(pos))
        return;

    for (auto window : renderWindows) {
        // ###: maybe should use QGuiApplication::topLevelAt
        if (window->geometry().contains(pos)) {
            setCurrentRenderWindow(window);
            break;
        }
    }
}

void WQuickCursorPrivate::setCurrentRenderWindow(WOutputRenderWindow *window)
{
    if (currentRenderWindow == window)
        return;
    currentRenderWindow = window;
    q_func()->WCursor::setEventWindow(window);
}

void WQuickCursorPrivate::onRenderWindowAdded(WOutputRenderWindow *window)
{
    if (!delegate)
        return;

    W_Q(WQuickCursor);
    auto obj = delegate->createWithInitialProperties({{"cursor", QVariant::fromValue(q)}}, qmlContext(q));
    auto item = qobject_cast<QQuickItem*>(obj);

    if (!item) {
        qWarning() << "Must using Item for the Cursor delegate";
        obj->deleteLater();
        return;
    }

    QQmlEngine::setObjectOwnership(item, QQmlEngine::CppOwnership);
    cursorItems.append(item);
    item->setZ(qreal(WOutputRenderWindow::Layer::Cursor));
    item->setParentItem(window->contentItem());
    item->setPosition(item->parentItem()->mapFromGlobal(q->position()));
}

void WQuickCursorPrivate::onRenderWindowRemoved(WOutputRenderWindow *window)
{
    for (auto item : cursorItems) {
        if (item->window() == window)
            item->deleteLater();
    }
}

void WQuickCursorPrivate::onCursorPositionChanged()
{
    // TODO: Auto hide the cursor item if it's a hardware cursor in current output.
    for (auto item : cursorItems)
        item->setPosition(item->parentItem()->mapFromGlobal(q_func()->position()));
    updateCurrentRenderWindow();
}

void WQuickCursorPrivate::updateXCursorManager()
{
    if (xcursor_manager) delete xcursor_manager;
    auto xm = QWXCursorManager::create(qPrintable(xcursorThemeName), getCursorSize());
    q_func()->setXCursorManager(xm);
}

WQuickCursor::WQuickCursor(QObject *parent)
    : WCursor(*new WQuickCursorPrivate(this), parent)
{
    connect(this, &WQuickCursor::cursorImageMaybeChanged, this, &WQuickCursor::cursorUrlChanged);
}

WQuickCursor::~WQuickCursor()
{
    W_DC(WQuickCursor);
    if (auto engine = qmlEngine(this))
        engine->removeImageProvider(d->imageProviderId());
}

WQuickOutputLayout *WQuickCursor::layout() const
{
    return static_cast<WQuickOutputLayout*>(WCursor::layout());
}

void WQuickCursor::setLayout(WQuickOutputLayout *layout)
{
    if (auto layout = this->layout()) {
        disconnect(layout, SIGNAL(outputsChanged()), this, SLOT(updateRenderWindows()));
    }

    WCursor::setLayout(layout);

    if (layout) {
        connect(layout, SIGNAL(outputsChanged()), this, SLOT(updateRenderWindows()));
    }

    W_D(WQuickCursor);

    if (d->componentComplete) {
        d->updateXCursorManager();
        d->updateRenderWindows();
    }
}

WOutputRenderWindow *WQuickCursor::currentRenderWindow() const
{
    W_DC(WQuickCursor);
    return d->currentRenderWindow;
}

QQmlComponent *WQuickCursor::delegate() const
{
    W_DC(WQuickCursor);
    return d->delegate;
}

void WQuickCursor::setDelegate(QQmlComponent *delegate)
{
    W_D(WQuickCursor);
    Q_ASSERT(!d->delegate);
    Q_ASSERT(delegate);

    d->delegate = delegate;

    if (d->componentComplete) {
        for (auto w : d->renderWindows)
            d->onRenderWindowAdded(w);
    }
}

QUrl WQuickCursor::cursorUrl() const
{
    QUrl url;
    url.setScheme("image");
    url.setHost(d_func()->imageProviderId());
    // TODO: Support animation

    return url;
}

QString WQuickCursor::themeName() const
{
    W_DC(WQuickCursor);
    return d->xcursorThemeName;
}

void WQuickCursor::setThemeName(const QString &name)
{
    W_D(WQuickCursor);

    if (d->xcursorThemeName == name)
        return;
    d->xcursorThemeName = name;
    QMetaObject::invokeMethod(this, SLOT(updateXCursorManager()), Qt::QueuedConnection);
}

QSize WQuickCursor::size() const
{
    W_DC(WQuickCursor);
    return d->cursorSize;
}

void WQuickCursor::setSize(const QSize &size)
{
    W_D(WQuickCursor);

    if (d->cursorSize == size)
        return;
    d->cursorSize = size;
    QMetaObject::invokeMethod(this, SLOT(updateXCursorManager()), Qt::QueuedConnection);
}

void WQuickCursor::move(QW_NAMESPACE::QWInputDevice *device, const QPointF &delta)
{
    W_D(WQuickCursor);
    WCursor::move(device, delta);
    d->onCursorPositionChanged();
}

void WQuickCursor::setPosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos)
{
    W_D(WQuickCursor);
    WCursor::setPosition(device, pos);
    d->onCursorPositionChanged();
}

bool WQuickCursor::setPositionWithChecker(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos)
{
    W_D(WQuickCursor);

    if (!WCursor::setPositionWithChecker(device, pos))
        return false;

    d->onCursorPositionChanged();
    return true;
}

void WQuickCursor::setScalePosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &ratio)
{
    W_D(WQuickCursor);
    WCursor::setScalePosition(device, ratio);
    d->onCursorPositionChanged();
}

void WQuickCursor::classBegin()
{
    W_D(WQuickCursor);
    d->componentComplete = false;
    qmlEngine(this)->addImageProvider(d_func()->imageProviderId(), new CursorImageProvider(this));
}

void WQuickCursor::componentComplete()
{
    W_D(WQuickCursor);

    if (layout()) {
        d->updateXCursorManager();
        d->updateRenderWindows();
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wquickcursor.cpp"

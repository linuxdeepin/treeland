// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WSurface>
#include <WSeat>
#include <WCursor>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QProcess>
#include <QTemporaryFile>
#include <QMouseEvent>
#include <QQuickItem>

#include <wquickbackend_p.h>
#include <qwbackend.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class MoveResizeHelper : public QObject {
    Q_OBJECT
    Q_PROPERTY(WSeat* seat READ seat WRITE setSeat NOTIFY seatChanged)

public:
    explicit MoveResizeHelper(QObject *parent = nullptr)
        : QObject(parent)
    {

    }

    Q_SLOT void startMove(WSurface *surface, int serial);
    Q_SLOT void startResize(WSurface *surface, Qt::Edges edge, int serial);

    inline void stop() {
        m_surfaceItem = nullptr;
        m_seat->setEventGrabber(nullptr);
        surface->notifyEndState(type);
        surface = nullptr;
    }

    bool event(QEvent *event) override;

    WSeat *seat() const;
    void setSeat(WSeat *newSeat);

    QQuickItem *surfaceItem() const;
    void setSurfaceItem(QQuickItem *newSurfaceItem);

signals:
    void seatChanged();
    void surfaceItemChanged();

private:
    WSurface::State type;
    WSurface *surface;
    QPointF surfacePosOfStartMoveResize;
    QSizeF surfaceSizeOfstartMoveResize;
    Qt::Edges resizeEdgets;

    WSeat *m_seat = nullptr;
    QQuickItem *m_surfaceItem = nullptr;
};

QQuickItem *MoveResizeHelper::surfaceItem() const
{
    return m_surfaceItem;
}

void MoveResizeHelper::setSurfaceItem(QQuickItem *newSurfaceItem)
{
    if (m_surfaceItem == newSurfaceItem)
        return;
    m_surfaceItem = newSurfaceItem;
    emit surfaceItemChanged();
}

WSeat *MoveResizeHelper::seat() const
{
    return m_seat;
}

void MoveResizeHelper::setSeat(WSeat *newSeat)
{
    if (m_seat == newSeat)
        return;
    m_seat = newSeat;
    emit seatChanged();
}

void MoveResizeHelper::startMove(WSurface *surface, int serial)
{
    Q_UNUSED(serial)
    if (!m_seat->cursor()
            || m_seat->cursor()->state() != Qt::LeftButton) {
        stop();
        return;
    }

    m_surfaceItem = qobject_cast<QQuickItem*>(surface->shell());
    if (!m_surfaceItem)
        return;

    type = WSurface::State::Move;
    this->surface = surface;
    surfacePosOfStartMoveResize = surface->position();

    surface->notifyBeginState(type);
    m_seat->setEventGrabber(this);
}

void MoveResizeHelper::startResize(WSurface *surface, Qt::Edges edge, int serial)
{
    Q_UNUSED(serial)
    if (!m_seat->cursor()
            || m_seat->cursor()->state() != Qt::LeftButton) {
        stop();
        return;
    }

    m_surfaceItem = qobject_cast<QQuickItem*>(surface->shell());
    if (!m_surfaceItem)
        return;

    type = WSurface::State::Resize;
    this->surface = surface;
    surfacePosOfStartMoveResize = surface->position();
    surfaceSizeOfstartMoveResize = surface->size();
    resizeEdgets = edge;

    surface->notifyBeginState(type);
    m_seat->setEventGrabber(this);
}

bool MoveResizeHelper::event(QEvent *event)
{
    if (Q_LIKELY(event->type() == QEvent::MouseMove)) {
        auto cursor = m_seat->cursor();
        Q_ASSERT(cursor);
        QMouseEvent *ev = static_cast<QMouseEvent*>(event);

        if (type == WSurface::State::Move) {
            auto increment_pos = ev->globalPos() - cursor->lastPressedPosition();
            auto new_pos = surfacePosOfStartMoveResize + increment_pos;
            m_surfaceItem->setPosition(new_pos);
        } else if (type == WSurface::State::Resize) {
            auto increment_pos = ev->globalPos() - cursor->lastPressedPosition();
            QRectF geo(surfacePosOfStartMoveResize, surfaceSizeOfstartMoveResize);

            if (resizeEdgets & Qt::LeftEdge)
                geo.setLeft(geo.left() + increment_pos.x());
            if (resizeEdgets & Qt::TopEdge)
                geo.setTop(geo.top() + increment_pos.y());

            if (resizeEdgets & Qt::RightEdge)
                geo.setRight(geo.right() + increment_pos.x());
            if (resizeEdgets & Qt::BottomEdge)
                geo.setBottom(geo.bottom() + increment_pos.y());

            m_surfaceItem->setPosition(geo.topLeft());
        } else {
            Q_ASSERT_X(false, __FUNCTION__, "Not support surface state request");
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        stop();
    } else {
        event->ignore();
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
//    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    WServer::initializeQPA();
    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    qmlRegisterModule("Tinywl", 1, 0);
    qmlRegisterType<MoveResizeHelper>("Tinywl", 1, 0, "MoveResizeHelper");

    QQmlApplicationEngine waylandEngine;
    waylandEngine.load(QUrl("qrc:/Main.qml"));
    WServer *server = waylandEngine.rootObjects().first()->findChild<WServer*>();
    Q_ASSERT(server);

    auto backend = server->findChild<WQuickBackend*>();
    Q_ASSERT(backend);

    // multi output
//    qobject_cast<QWMultiBackend*>(backend->backend())->forEachBackend([] (wlr_backend *backend, void *) {
//        if (auto x11 = QWX11Backend::from(backend))
//            x11->createOutput();
//    }, nullptr);

    QTemporaryFile tmpQmlFile;
    tmpQmlFile.setAutoRemove(true);
    QProcess waylandClientDemo;

    if (tmpQmlFile.open()) {
        QFile qmlFile(":/ClientWindow.qml");
        if (qmlFile.open(QIODevice::ReadOnly)) {
            tmpQmlFile.write(qmlFile.readAll());
        }
        tmpQmlFile.close();

        waylandClientDemo.setProgram("qml");
        waylandClientDemo.setArguments({tmpQmlFile.fileName(), "-platform", "wayland"});
        server->startProcess(waylandClientDemo);
    }

    return app.exec();
}

#include "main.moc"

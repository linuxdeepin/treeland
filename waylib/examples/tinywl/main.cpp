// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>
#include <WSurface>
#include <WSeat>
#include <WCursor>
#include <wsocket.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QProcess>
#include <QTemporaryFile>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <wquickbackend_p.h>
#include <qwbackend.h>
#include <qwdisplay.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class Helper : public WSeatEventFilter {
    Q_OBJECT

public:
    explicit Helper(QObject *parent = nullptr)
        : WSeatEventFilter(parent)
    {

    }

    Q_SLOT void startMove(WSurface *surface, WSeat *seat, int serial);
    Q_SLOT void startResize(WSurface *surface, WSeat *seat, Qt::Edges edge, int serial);
    Q_SLOT bool startDemoClient(const QString &socket);

    inline void stop() {
        surfaceShellItem = nullptr;
        surface->notifyEndState(type);
        surface = nullptr;
        seat = nullptr;
    }

private:
    bool eventFilter(WSeat *seat, QWindow *watched, QInputEvent *event) override;
    bool eventFilter(WSeat *seat, WSurface *watched, QInputEvent *event) override;
    bool ignoredEventFilter(WSeat *seat, QWindow *watched, QInputEvent *event) override;

    // for move resize
    WSurface::State type;
    QPointer<WSurface> surface;
    WSeat *seat = nullptr;
    QPointF surfacePosOfStartMoveResize;
    QSizeF surfaceSizeOfstartMoveResize;
    Qt::Edges resizeEdgets;
    QPointer<QQuickItem> surfaceShellItem;
};

inline QPointF getItemGlobalPosition(QQuickItem *item)
{
    auto parent = item->parentItem();
    return parent ? parent->mapToGlobal(item->position()) : item->position();
}

void Helper::startMove(WSurface *surface, WSeat *seat, int serial)
{
    Q_UNUSED(serial)

    surfaceShellItem = qobject_cast<QQuickItem*>(surface->shell());
    if (!surfaceShellItem)
        return;

    type = WSurface::State::Move;
    this->surface = surface;
    this->seat = seat;
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceShellItem);

    surface->notifyBeginState(type);
}

void Helper::startResize(WSurface *surface, WSeat *seat, Qt::Edges edge, int serial)
{
    Q_UNUSED(serial)

    surfaceShellItem = qobject_cast<QQuickItem*>(surface->shell());
    if (!surfaceShellItem)
        return;

    type = WSurface::State::Resize;
    this->surface = surface;
    this->seat = seat;
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceShellItem);
    surfaceSizeOfstartMoveResize = surfaceShellItem->size();
    resizeEdgets = edge;

    surface->notifyBeginState(type);
}

bool Helper::startDemoClient(const QString &socket)
{
    QTemporaryFile *tmpQmlFile = new QTemporaryFile();
    tmpQmlFile->setParent(qApp);
    tmpQmlFile->setAutoRemove(true);
    QProcess waylandClientDemo;

    if (tmpQmlFile->open()) {
        QFile qmlFile(":/ClientWindow.qml");
        if (qmlFile.open(QIODevice::ReadOnly)) {
            tmpQmlFile->write(qmlFile.readAll());
        }
        tmpQmlFile->close();

        waylandClientDemo.setProgram("gedit");
//        waylandClientDemo.setArguments({tmpQmlFile->fileName(), "-platform", "wayland"});

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WAYLAND_DISPLAY", socket);

        waylandClientDemo.setProcessEnvironment(env);
        return waylandClientDemo.startDetached();
    }

    return false;
}

bool Helper::eventFilter(WSeat *seat, QWindow *watched, QInputEvent *event)
{
    if (watched) {
        if (event->type() == QEvent::MouseButtonPress) {
            seat->setKeyboardFocusTarget(watched);
        } else if (event->type() == QEvent::MouseMove && !seat->focusWindow()) {
            seat->setKeyboardFocusTarget(watched);
        }
    }

    return false;
}

bool Helper::eventFilter(WSeat *seat, WSurface *watched, QInputEvent *event)
{
    if (watched && event->type() == QEvent::MouseButtonRelease) {
        seat->setKeyboardFocusTarget(watched);

        if (auto item = qobject_cast<QQuickItem*>(watched->shell()))
            item->forceActiveFocus(Qt::MouseFocusReason);
    }

    if (surfaceShellItem && watched->shell() == surfaceShellItem.get()
        && seat == this->seat) {
        // for move resize
        if (Q_LIKELY(event->type() == QEvent::MouseMove)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent*>(event);

            if (type == WSurface::State::Move) {
                auto increment_pos = ev->globalPosition() - cursor->lastPressedPosition();
                auto new_pos = surfacePosOfStartMoveResize + surfaceShellItem->parentItem()->mapFromGlobal(increment_pos);
                surfaceShellItem->setPosition(new_pos);
            } else if (type == WSurface::State::Resize) {
                auto increment_pos = surfaceShellItem->parentItem()->mapFromGlobal(ev->globalPosition() - cursor->lastPressedPosition());
                QRectF geo(surfacePosOfStartMoveResize, surfaceSizeOfstartMoveResize);

                if (resizeEdgets & Qt::LeftEdge)
                    geo.setLeft(geo.left() + increment_pos.x());
                if (resizeEdgets & Qt::TopEdge)
                    geo.setTop(geo.top() + increment_pos.y());

                if (resizeEdgets & Qt::RightEdge)
                    geo.setRight(geo.right() + increment_pos.x());
                if (resizeEdgets & Qt::BottomEdge)
                    geo.setBottom(geo.bottom() + increment_pos.y());

                surfaceShellItem->setPosition(geo.topLeft());
            } else {
                Q_ASSERT_X(false, __FUNCTION__, "Not support surface state request");
            }

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            stop();
            return false;
        }
    }

    return false;
}

bool Helper::ignoredEventFilter(WSeat *seat, QWindow *watched, QInputEvent *event)
{
    if (watched && event->type() == QEvent::MouseButtonPress) {
        // clear focus
        if (auto fs = seat->keyboardFocusSurface()) {
            if (auto item = qobject_cast<QQuickItem*>(fs->shell()))
                item->setFocus(false);
            seat->setKeyboardFocusTarget(static_cast<WSurface*>(nullptr));
        }
    }

    return false;
}

int main(int argc, char *argv[]) {
//    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    WServer::initializeQPA();
    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    qmlRegisterModule("Tinywl", 1, 0);
    qmlRegisterType<Helper>("Tinywl", 1, 0, "Helper");

    QQmlApplicationEngine waylandEngine;
    waylandEngine.load(QUrl("qrc:/Main.qml"));
    WServer *server = waylandEngine.rootObjects().first()->findChild<WServer*>();
    Q_ASSERT(server);
    Q_ASSERT(server->isRunning());

    auto backend = server->findChild<WQuickBackend*>();
    Q_ASSERT(backend);

    // multi output
//    qobject_cast<QWMultiBackend*>(backend->backend())->forEachBackend([] (wlr_backend *backend, void *) {
//        if (auto x11 = QWX11Backend::from(backend))
//            x11->createOutput();
//    }, nullptr);


    return app.exec();
}

#include "main.moc"

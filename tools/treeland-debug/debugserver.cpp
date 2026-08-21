// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debugserver.h"

#include <QHostAddress>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QWebSocket>
#include <QTimer>

#include <QHttpHeaders>

#include <functional>
#include <algorithm>

DebugServer::DebugServer(const QString &url, const QString &name, int timeoutMs,
                         QObject *parent)
    : QObject(parent)
    , m_url(url)
    , m_name(name)
    , m_timeoutMs(timeoutMs)
{
}

bool DebugServer::createSession(Session &session)
{
    return connectSession(session, m_url, m_name, m_timeoutMs);
}

QJsonObject DebugServer::sessionRequest(const std::function<QJsonObject(Session &)> &work)
{
    Session session;
    if (!createSession(session))
        return {{"ok", false}, {"error", QStringLiteral("failed to connect to remote object node: %1").arg(m_url)}};

    return work(session);
}

// Adds CORS headers to a response so a browser-based frontend can call the
// API directly.
static QHttpServerResponse addCorsHeaders(QHttpServerResponse response)
{
    auto headers = response.headers();
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::AccessControlAllowOrigin, "*");
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::AccessControlAllowMethods,
                            "GET, POST, OPTIONS");
    headers.replaceOrAppend(QHttpHeaders::WellKnownHeader::AccessControlAllowHeaders,
                            "Content-Type");
    response.setHeaders(std::move(headers));
    return response;
}

// Wraps a QJsonObject-producing handler so its response includes CORS headers.
// QHttpServer auto-serializes QJsonObject to application/json.
static QHttpServerResponse jsonCorsResponse(const QJsonObject &obj,
                                            QHttpServerResponse::StatusCode status = QHttpServerResponse::StatusCode::Ok)
{
    return addCorsHeaders(QHttpServerResponse(obj, status));
}

bool DebugServer::listen(const QString &host, int port)
{
    auto *tcpServer = new QTcpServer(this);
    if (!tcpServer->listen(QHostAddress(host), port)) {
        delete tcpServer;
        return false;
    }
    if (!m_httpServer.bind(tcpServer)) {
        delete tcpServer;
        return false;
    }

    // Allow WebSocket upgrades on any path — clients connect to /ws.
    m_httpServer.addWebSocketUpgradeVerifier(this, [](const QHttpServerRequest &) {
        return QHttpServerWebSocketUpgradeResponse::accept();
    });

    // --- Inspection routes ---
    m_httpServer.route("/api/tree", [this]() {
        return jsonCorsResponse(handleTree());
    });
    m_httpServer.route("/api/cursor", [this]() {
        return jsonCorsResponse(handleCursor());
    });
    m_httpServer.route("/api/windows", [this]() {
        return jsonCorsResponse(handleWindows());
    });
    m_httpServer.route("/api/clients", [this]() {
        return jsonCorsResponse(handleClients());
    });
    m_httpServer.route("/api/focused", [this]() {
        return jsonCorsResponse(handleFocused());
    });
    m_httpServer.route("/api/cursor-window", [this]() {
        return jsonCorsResponse(handleCursorWindow());
    });

    // --- Window control routes ---
    m_httpServer.route("/api/activate", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleActivate(request));
    });
    m_httpServer.route("/api/close", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleClose(request));
    });
    m_httpServer.route("/api/minimize", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleMinimize(request));
    });
    m_httpServer.route("/api/maximize", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleMaximize(request));
    });
    m_httpServer.route("/api/fullscreen", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleFullscreen(request));
    });
    m_httpServer.route("/api/move", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleMove(request));
    });
    m_httpServer.route("/api/resize", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleResize(request));
    });
    m_httpServer.route("/api/workspace", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleWorkspace(request));
    });

    // --- Input / event routes ---
    m_httpServer.route("/api/move-cursor", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleMoveCursor(request));
    });
    m_httpServer.route("/api/event/motion", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleEventMotion(request));
    });
    m_httpServer.route("/api/event/button", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleEventButton(request));
    });
    m_httpServer.route("/api/event/key", QHttpServerRequest::Method::Post,
                       [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleEventKey(request));
    });
    m_httpServer.route("/api/events", [this](const QHttpServerRequest &request) {
        return jsonCorsResponse(handleEvents(request));
    });

    // --- Image capture routes (return raw PNG bytes) ---
    m_httpServer.route("/api/screenshot/output", [this](const QHttpServerRequest &request) {
        return addCorsHeaders(handleScreenshotOutput(request));
    });
    m_httpServer.route("/api/screenshot/window", [this](const QHttpServerRequest &request) {
        return addCorsHeaders(handleScreenshotWindow(request));
    });

    // --- CORS preflight handler ---
    // Any OPTIONS request returns 204 with CORS headers so browsers can
    // pre-flight before actual API calls.  Other unmatched paths return a
    // 404 JSON body.
    m_httpServer.setMissingHandler(this, [](const QHttpServerRequest &request) {
        if (request.method() == QHttpServerRequest::Method::Options)
            return addCorsHeaders(QHttpServerResponse(QHttpServerResponse::StatusCode::NoContent));
        return addCorsHeaders(QHttpServerResponse(
            QJsonObject{{"ok", false}, {"error", "not found"}},
            QHttpServerResponse::StatusCode::NotFound));
    });

    connect(&m_httpServer, &QAbstractHttpServer::newWebSocketConnection,
            this, &DebugServer::onNewWebSocketConnection);

    return true;
}

// --- Inspection handlers ---

QJsonObject DebugServer::handleTree()
{
    return sessionRequest([this](Session &session) {
        TreelandInfo info;
        if (!waitSlot(session.replica->getTreelandInfo(), m_timeoutMs, &info))
            return QJsonObject{{"ok", false}, {"error", "getTreelandInfo() failed"}};
        return QJsonObject{{"ok", true}, {"data", treelandInfoToJson(info)}};
    });
}

QJsonObject DebugServer::handleCursor()
{
    return sessionRequest([this](Session &session) {
        QPointF pos = session.replica->cursorPosition();
        return QJsonObject{{"ok", true}, {"data", pointToJson(pos)}};
    });
}

QJsonObject DebugServer::handleWindows()
{
    return sessionRequest([this](Session &session) {
        QList<WindowInfo> windows;
        if (!waitSlot(session.replica->getWindows(), m_timeoutMs, &windows))
            return QJsonObject{{"ok", false}, {"error", "getWindows() failed"}};
        return QJsonObject{{"ok", true}, {"data", windowsToJson(windows)}};
    });
}

QJsonObject DebugServer::handleClients()
{
    return sessionRequest([this](Session &session) {
        QList<ClientInfo> clients;
        if (!waitSlot(session.replica->getClients(), m_timeoutMs, &clients))
            return QJsonObject{{"ok", false}, {"error", "getClients() failed"}};
        return QJsonObject{{"ok", true}, {"data", clientsToJson(clients)}};
    });
}

QJsonObject DebugServer::handleFocused()
{
    return sessionRequest([this](Session &session) {
        qint64 id = 0;
        if (!waitSlot(session.replica->focusedWindowId(), m_timeoutMs, &id))
            return QJsonObject{{"ok", false}, {"error", "focusedWindowId() failed"}};
        return QJsonObject{{"ok", true}, {"data", id}};
    });
}

QJsonObject DebugServer::handleCursorWindow()
{
    return sessionRequest([this](Session &session) {
        qint64 id = 0;
        if (!waitSlot(session.replica->windowUnderCursor(), m_timeoutMs, &id))
            return QJsonObject{{"ok", false}, {"error", "windowUnderCursor() failed"}};
        return QJsonObject{{"ok", true}, {"data", id}};
    });
}

// --- Window control handlers ---

// Resolves a target token (numeric id or appId) to a window id inside a
// sessionRequest lambda. Returns {ok, id} or {ok:false, error}.
static QJsonObject resolveAndControl(Session &session, int timeoutMs,
                                     const QString &target,
                                     const std::function<bool(qint64)> &action)
{
    bool ok = false;
    const qint64 id = resolveTarget(session, timeoutMs, target, &ok);
    if (!ok)
        return {{"ok", false}, {"error", QStringLiteral("no window matches '%1'").arg(target)}};
    bool result = action(id);
    return {{"ok", true}, {"data", result}};
}

QJsonObject DebugServer::handleActivate(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString target = body.value("target").toString();
    if (target.isEmpty())
        return {{"ok", false}, {"error", "missing 'target'"}};
    return sessionRequest([this, target](Session &session) {
        return resolveAndControl(session, m_timeoutMs, target, [this, &session](qint64 id) {
            bool result = false;
            waitSlot(session.replica->activateWindow(id), m_timeoutMs, &result);
            return result;
        });
    });
}

QJsonObject DebugServer::handleClose(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString target = body.value("target").toString();
    if (target.isEmpty())
        return {{"ok", false}, {"error", "missing 'target'"}};
    return sessionRequest([this, target](Session &session) {
        return resolveAndControl(session, m_timeoutMs, target, [this, &session](qint64 id) {
            bool result = false;
            waitSlot(session.replica->closeWindow(id), m_timeoutMs, &result);
            return result;
        });
    });
}

QJsonObject DebugServer::handleMinimize(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString target = body.value("target").toString();
    if (target.isEmpty())
        return {{"ok", false}, {"error", "missing 'target'"}};
    return sessionRequest([this, target](Session &session) {
        return resolveAndControl(session, m_timeoutMs, target, [this, &session](qint64 id) {
            bool result = false;
            waitSlot(session.replica->minimizeWindow(id), m_timeoutMs, &result);
            return result;
        });
    });
}

QJsonObject DebugServer::handleMaximize(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString target = body.value("target").toString();
    if (target.isEmpty())
        return {{"ok", false}, {"error", "missing 'target'"}};
    return sessionRequest([this, target](Session &session) {
        return resolveAndControl(session, m_timeoutMs, target, [this, &session](qint64 id) {
            bool result = false;
            waitSlot(session.replica->toggleMaximized(id), m_timeoutMs, &result);
            return result;
        });
    });
}

QJsonObject DebugServer::handleFullscreen(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString target = body.value("target").toString();
    if (target.isEmpty())
        return {{"ok", false}, {"error", "missing 'target'"}};
    return sessionRequest([this, target](Session &session) {
        return resolveAndControl(session, m_timeoutMs, target, [this, &session](qint64 id) {
            bool result = false;
            waitSlot(session.replica->toggleFullscreen(id), m_timeoutMs, &result);
            return result;
        });
    });
}

QJsonObject DebugServer::handleMove(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString target = body.value("target").toString();
    if (target.isEmpty())
        return {{"ok", false}, {"error", "missing 'target'"}};
    const int x = body.value("x").toInt();
    const int y = body.value("y").toInt();
    return sessionRequest([this, target, x, y](Session &session) {
        return resolveAndControl(session, m_timeoutMs, target, [this, &session, x, y](qint64 id) {
            bool result = false;
            waitSlot(session.replica->moveWindow(id, x, y), m_timeoutMs, &result);
            return result;
        });
    });
}

QJsonObject DebugServer::handleResize(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString target = body.value("target").toString();
    if (target.isEmpty())
        return {{"ok", false}, {"error", "missing 'target'"}};
    const int width = body.value("width").toInt();
    const int height = body.value("height").toInt();
    return sessionRequest([this, target, width, height](Session &session) {
        return resolveAndControl(session, m_timeoutMs, target, [this, &session, width, height](qint64 id) {
            bool result = false;
            waitSlot(session.replica->resizeWindow(id, width, height), m_timeoutMs, &result);
            return result;
        });
    });
}

QJsonObject DebugServer::handleWorkspace(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString target = body.value("target").toString();
    if (target.isEmpty())
        return {{"ok", false}, {"error", "missing 'target'"}};
    const int workspaceId = body.value("workspaceId").toInt();
    return sessionRequest([this, target, workspaceId](Session &session) {
        return resolveAndControl(session, m_timeoutMs, target, [this, &session, workspaceId](qint64 id) {
            bool result = false;
            waitSlot(session.replica->setWindowWorkspace(id, workspaceId), m_timeoutMs, &result);
            return result;
        });
    });
}

// --- Input / event handlers ---

QJsonObject DebugServer::handleMoveCursor(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    if (!body.contains("x") || !body.contains("y"))
        return {{"ok", false}, {"error", "missing 'x' or 'y'"}};
    const double x = body.value("x").toDouble();
    const double y = body.value("y").toDouble();
    return sessionRequest([this, x, y](Session &session) {
        bool result = false;
        if (!waitSlot(session.replica->moveCursor(QPointF(x, y)), m_timeoutMs, &result))
            return QJsonObject{{"ok", false}, {"error", "moveCursor() failed"}};
        return QJsonObject{{"ok", true}, {"data", result}};
    });
}

QJsonObject DebugServer::handleEventMotion(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    if (!body.contains("x") || !body.contains("y"))
        return {{"ok", false}, {"error", "missing 'x' or 'y'"}};
    const double x = body.value("x").toDouble();
    const double y = body.value("y").toDouble();
    return sessionRequest([this, x, y](Session &session) {
        bool result = false;
        if (!waitSlot(session.replica->moveCursor(QPointF(x, y)), m_timeoutMs, &result))
            return QJsonObject{{"ok", false}, {"error", "moveCursor() failed"}};
        return QJsonObject{{"ok", true}, {"data", result}};
    });
}

QJsonObject DebugServer::handleEventButton(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString buttonName = body.value("button").toString();
    if (buttonName.isEmpty())
        return {{"ok", false}, {"error", "missing 'button'"}};
    bool buttonOk = false;
    const int code = buttonCode(buttonName, &buttonOk);
    if (!buttonOk)
        return {{"ok", false}, {"error", QStringLiteral("unknown button '%1'").arg(buttonName)}};
    const QString action = body.value("action").toString("click");
    return sessionRequest([this, code, action](Session &session) {
        bool result = false;
        if (action == "press") {
            if (!waitSlot(session.replica->sendPointerButton(code, true), m_timeoutMs, &result))
                return QJsonObject{{"ok", false}, {"error", "sendPointerButton() failed"}};
        } else if (action == "release") {
            if (!waitSlot(session.replica->sendPointerButton(code, false), m_timeoutMs, &result))
                return QJsonObject{{"ok", false}, {"error", "sendPointerButton() failed"}};
        } else {
            bool r1 = false, r2 = false;
            if (!waitSlot(session.replica->sendPointerButton(code, true), m_timeoutMs, &r1))
                return QJsonObject{{"ok", false}, {"error", "sendPointerButton() failed"}};
            if (!waitSlot(session.replica->sendPointerButton(code, false), m_timeoutMs, &r2))
                return QJsonObject{{"ok", false}, {"error", "sendPointerButton() failed"}};
            result = r1 && r2;
        }
        return QJsonObject{{"ok", true}, {"data", result}};
    });
}

QJsonObject DebugServer::handleEventKey(const QHttpServerRequest &request)
{
    const auto body = QJsonDocument::fromJson(request.body()).object();
    const QString keyName = body.value("key").toString();
    if (keyName.isEmpty())
        return {{"ok", false}, {"error", "missing 'key'"}};
    bool keyOk = false;
    const int code = keyCode(keyName, &keyOk);
    if (!keyOk)
        return {{"ok", false}, {"error", QStringLiteral("unknown key '%1'").arg(keyName)}};
    const QString action = body.value("action").toString("tap");
    return sessionRequest([this, code, action](Session &session) {
        bool result = false;
        if (action == "press") {
            if (!waitSlot(session.replica->sendKey(code, true), m_timeoutMs, &result))
                return QJsonObject{{"ok", false}, {"error", "sendKey() failed"}};
        } else if (action == "release") {
            if (!waitSlot(session.replica->sendKey(code, false), m_timeoutMs, &result))
                return QJsonObject{{"ok", false}, {"error", "sendKey() failed"}};
        } else {
            bool r1 = false, r2 = false;
            if (!waitSlot(session.replica->sendKey(code, true), m_timeoutMs, &r1))
                return QJsonObject{{"ok", false}, {"error", "sendKey() failed"}};
            if (!waitSlot(session.replica->sendKey(code, false), m_timeoutMs, &r2))
                return QJsonObject{{"ok", false}, {"error", "sendKey() failed"}};
            result = r1 && r2;
        }
        return QJsonObject{{"ok", true}, {"data", result}};
    });
}

QJsonObject DebugServer::handleEvents(const QHttpServerRequest &request)
{
    const auto query = request.query();
    quint64 since = query.queryItemValue("since").toULongLong();
    return sessionRequest([this, since](Session &session) {
        QList<DebugEvent> events;
        if (!waitSlot(session.replica->getEvents(since), m_timeoutMs, &events))
            return QJsonObject{{"ok", false}, {"error", "getEvents() failed"}};
        return QJsonObject{{"ok", true}, {"data", debugEventsToJson(events)}};
    });
}

// --- Image capture handlers ---

QHttpServerResponse DebugServer::handleScreenshotOutput(const QHttpServerRequest &request)
{
    const auto query = request.query();
    const QString outputName = query.queryItemValue("name");
    Session session;
    if (!createSession(session))
        return QHttpServerResponse(
            QJsonObject{{"ok", false}, {"error", "failed to connect"}},
            QHttpServerResponse::StatusCode::ServiceUnavailable);

    QByteArray data;
    if (!waitSlot(session.replica->captureOutput(outputName), m_timeoutMs, &data) || data.isEmpty())
        return QHttpServerResponse(
            QJsonObject{{"ok", false}, {"error", "captureOutput: no image produced"}},
            QHttpServerResponse::StatusCode::NotFound);

    return QHttpServerResponse("image/png", data);
}

QHttpServerResponse DebugServer::handleScreenshotWindow(const QHttpServerRequest &request)
{
    const auto query = request.query();
    const QString target = query.queryItemValue("target");
    if (target.isEmpty())
        return QHttpServerResponse(
            QJsonObject{{"ok", false}, {"error", "missing 'target' query parameter"}},
            QHttpServerResponse::StatusCode::BadRequest);

    Session session;
    if (!createSession(session))
        return QHttpServerResponse(
            QJsonObject{{"ok", false}, {"error", "failed to connect"}},
            QHttpServerResponse::StatusCode::ServiceUnavailable);

    bool ok = false;
    const qint64 id = resolveTarget(session, m_timeoutMs, target, &ok);
    if (!ok)
        return QHttpServerResponse(
            QJsonObject{{"ok", false}, {"error", QStringLiteral("no window matches '%1'").arg(target)}},
            QHttpServerResponse::StatusCode::NotFound);

    QByteArray data;
    if (!waitSlot(session.replica->captureWindow(id), m_timeoutMs, &data) || data.isEmpty())
        return QHttpServerResponse(
            QJsonObject{{"ok", false}, {"error", "captureWindow: no image produced"}},
            QHttpServerResponse::StatusCode::NotFound);

    return QHttpServerResponse("image/png", data);
}

// --- WebSocket handling ---

void DebugServer::onNewWebSocketConnection()
{
    auto socket = m_httpServer.nextPendingWebSocketConnection();
    if (!socket)
        return;

    QWebSocket *ws = socket.release();
    ws->setParent(this);
    connect(ws, &QWebSocket::textMessageReceived, this, [this, ws](const QString &message) {
        handleWebSocketMessage(ws, message);
    });
    connect(ws, &QWebSocket::disconnected, this, [this, ws]() {
        stopLiveSubscription(ws, QString());
        ws->deleteLater();
    });
}

void DebugServer::handleWebSocketMessage(QWebSocket *socket, const QString &message)
{
    const auto doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(
            QJsonObject{{"ok", false}, {"error", "expected a JSON object"}}).toJson()));
        return;
    }

    const auto obj = doc.object();
    const QString command = obj.value("command").toString();
    const QString id = obj.value("id").toString();

    // Live subscriptions: subscribe/unsubscribe
    if (command == "subscribe") {
        const QString sub = obj.value("type").toString();
        int intervalMs = obj.value("intervalMs").toInt(1000);
        if (intervalMs <= 0)
            intervalMs = 1000;

        if (sub == "top") {
            startLiveSubscription(socket, id, DebugCommand::Top, intervalMs);
        } else if (sub == "events") {
            startLiveSubscription(socket, id, DebugCommand::Events, intervalMs);
        } else if (sub == "watch") {
            const QString target = obj.value("target").toString();
            if (target.isEmpty()) {
                socket->sendTextMessage(QString::fromUtf8(QJsonDocument(
                    QJsonObject{{"ok", false}, {"id", id}, {"error", "watch requires 'target'"}}).toJson()));
                return;
            }
            // Resolve target now and store the resolved id for the subscription.
            Session session;
            if (!createSession(session)) {
                socket->sendTextMessage(QString::fromUtf8(QJsonDocument(
                    QJsonObject{{"ok", false}, {"id", id}, {"error", "failed to connect"}}).toJson()));
                return;
            }
            bool ok = false;
            const qint64 windowId = resolveTarget(session, m_timeoutMs, target, &ok);
            if (!ok) {
                socket->sendTextMessage(QString::fromUtf8(QJsonDocument(
                    QJsonObject{{"ok", false}, {"id", id}, {"error", QStringLiteral("no window matches '%1'").arg(target)}}).toJson()));
                return;
            }
            // Use a special subscription type for watch with the resolved id.
            // Store in a property on the socket for the timer to read.
            socket->setProperty("watchId", windowId);
            startLiveSubscription(socket, id, DebugCommand::Watch, intervalMs);
        } else {
            socket->sendTextMessage(QString::fromUtf8(QJsonDocument(
                QJsonObject{{"ok", false}, {"id", id}, {"error", "unknown subscription type"}}).toJson()));
        }
        return;
    }

    if (command == "unsubscribe") {
        stopLiveSubscription(socket, id);
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(
            QJsonObject{{"ok", true}, {"id", id}, {"data", "unsubscribed"}}).toJson()));
        return;
    }

    // One-shot commands via WebSocket — reuse the same sessionRequest logic.
    QJsonObject result;
    if (command == "tree") {
        result = handleTree();
    } else if (command == "cursor") {
        result = handleCursor();
    } else if (command == "windows") {
        result = handleWindows();
    } else if (command == "clients") {
        result = handleClients();
    } else if (command == "focused") {
        result = handleFocused();
    } else if (command == "cursor-window") {
        result = handleCursorWindow();
    } else if (command == "activate" || command == "close" || command == "minimize"
               || command == "maximize" || command == "fullscreen" || command == "move"
               || command == "resize" || command == "workspace") {
        // These need a target and optional params from the message body.
        const QHttpServerRequest *fakeRequest = nullptr;
        (void)fakeRequest;
        // Build a ParseResult-like path: reuse sessionRequest with the params.
        result = sessionRequest([this, command, obj](Session &session) {
            const QString target = obj.value("target").toString();
            if (target.isEmpty())
                return QJsonObject{{"ok", false}, {"error", "missing 'target'"}};
            bool ok = false;
            const qint64 winId = resolveTarget(session, m_timeoutMs, target, &ok);
            if (!ok)
                return QJsonObject{{"ok", false}, {"error", QStringLiteral("no window matches '%1'").arg(target)}};
            bool res = false;
            if (command == "activate")
                waitSlot(session.replica->activateWindow(winId), m_timeoutMs, &res);
            else if (command == "close")
                waitSlot(session.replica->closeWindow(winId), m_timeoutMs, &res);
            else if (command == "minimize")
                waitSlot(session.replica->minimizeWindow(winId), m_timeoutMs, &res);
            else if (command == "maximize")
                waitSlot(session.replica->toggleMaximized(winId), m_timeoutMs, &res);
            else if (command == "fullscreen")
                waitSlot(session.replica->toggleFullscreen(winId), m_timeoutMs, &res);
            else if (command == "move") {
                const int x = obj.value("x").toInt();
                const int y = obj.value("y").toInt();
                waitSlot(session.replica->moveWindow(winId, x, y), m_timeoutMs, &res);
            } else if (command == "resize") {
                const int w = obj.value("width").toInt();
                const int h = obj.value("height").toInt();
                waitSlot(session.replica->resizeWindow(winId, w, h), m_timeoutMs, &res);
            } else if (command == "workspace") {
                const int wsId = obj.value("workspaceId").toInt();
                waitSlot(session.replica->setWindowWorkspace(winId, wsId), m_timeoutMs, &res);
            }
            return QJsonObject{{"ok", true}, {"data", res}};
        });
    } else if (command == "move-cursor" || command == "event-motion") {
        const double x = obj.value("x").toDouble();
        const double y = obj.value("y").toDouble();
        result = sessionRequest([this, x, y](Session &session) {
            bool res = false;
            if (!waitSlot(session.replica->moveCursor(QPointF(x, y)), m_timeoutMs, &res))
                return QJsonObject{{"ok", false}, {"error", "moveCursor() failed"}};
            return QJsonObject{{"ok", true}, {"data", res}};
        });
    } else if (command == "event-button") {
        const QString buttonName = obj.value("button").toString();
        if (buttonName.isEmpty()) {
            result = {{"ok", false}, {"error", "missing 'button'"}};
        } else {
            bool btnOk = false;
            const int code = buttonCode(buttonName, &btnOk);
            if (!btnOk) {
                result = {{"ok", false}, {"error", QStringLiteral("unknown button '%1'").arg(buttonName)}};
            } else {
                const QString action = obj.value("action").toString("click");
                result = sessionRequest([this, code, action](Session &session) {
                    bool res = false;
                    if (action == "press") {
                        waitSlot(session.replica->sendPointerButton(code, true), m_timeoutMs, &res);
                    } else if (action == "release") {
                        waitSlot(session.replica->sendPointerButton(code, false), m_timeoutMs, &res);
                    } else {
                        bool r1 = false, r2 = false;
                        waitSlot(session.replica->sendPointerButton(code, true), m_timeoutMs, &r1);
                        waitSlot(session.replica->sendPointerButton(code, false), m_timeoutMs, &r2);
                        res = r1 && r2;
                    }
                    return QJsonObject{{"ok", true}, {"data", res}};
                });
            }
        }
    } else if (command == "event-key") {
        const QString keyName = obj.value("key").toString();
        if (keyName.isEmpty()) {
            result = {{"ok", false}, {"error", "missing 'key'"}};
        } else {
            bool keyOk = false;
            const int code = keyCode(keyName, &keyOk);
            if (!keyOk) {
                result = {{"ok", false}, {"error", QStringLiteral("unknown key '%1'").arg(keyName)}};
            } else {
                const QString action = obj.value("action").toString("tap");
                result = sessionRequest([this, code, action](Session &session) {
                    bool res = false;
                    if (action == "press") {
                        waitSlot(session.replica->sendKey(code, true), m_timeoutMs, &res);
                    } else if (action == "release") {
                        waitSlot(session.replica->sendKey(code, false), m_timeoutMs, &res);
                    } else {
                        bool r1 = false, r2 = false;
                        waitSlot(session.replica->sendKey(code, true), m_timeoutMs, &r1);
                        waitSlot(session.replica->sendKey(code, false), m_timeoutMs, &r2);
                        res = r1 && r2;
                    }
                    return QJsonObject{{"ok", true}, {"data", res}};
                });
            }
        }
    } else if (command == "events") {
        const quint64 since = obj.value("since").toVariant().toULongLong();
        result = sessionRequest([this, since](Session &session) {
            QList<DebugEvent> events;
            if (!waitSlot(session.replica->getEvents(since), m_timeoutMs, &events))
                return QJsonObject{{"ok", false}, {"error", "getEvents() failed"}};
            return QJsonObject{{"ok", true}, {"data", debugEventsToJson(events)}};
        });
    } else if (command == "screenshot-output") {
        const QString outputName = obj.value("name").toString();
        Session session;
        if (!createSession(session)) {
            result = {{"ok", false}, {"error", "failed to connect"}};
        } else {
            QByteArray data;
            if (!waitSlot(session.replica->captureOutput(outputName), m_timeoutMs, &data) || data.isEmpty()) {
                result = {{"ok", false}, {"error", "captureOutput: no image produced"}};
            } else {
                // Send as binary message, then a small JSON ack so the client
                // can correlate via id.
                socket->sendBinaryMessage(data);
                result = {{"ok", true}, {"id", id}, {"data", "image sent as binary"}};
            }
        }
    } else if (command == "screenshot-window") {
        const QString target = obj.value("target").toString();
        if (target.isEmpty()) {
            result = {{"ok", false}, {"error", "missing 'target'"}};
        } else {
            Session session;
            if (!createSession(session)) {
                result = {{"ok", false}, {"error", "failed to connect"}};
            } else {
                bool ok = false;
                const qint64 winId = resolveTarget(session, m_timeoutMs, target, &ok);
                if (!ok) {
                    result = {{"ok", false}, {"error", QStringLiteral("no window matches '%1'").arg(target)}};
                } else {
                    QByteArray data;
                    if (!waitSlot(session.replica->captureWindow(winId), m_timeoutMs, &data) || data.isEmpty()) {
                        result = {{"ok", false}, {"error", "captureWindow: no image produced"}};
                    } else {
                        socket->sendBinaryMessage(data);
                        result = {{"ok", true}, {"id", id}, {"data", "image sent as binary"}};
                    }
                }
            }
        }
    } else {
        result = {{"ok", false}, {"error", QStringLiteral("unknown command '%1'").arg(command)}};
    }

    if (!id.isEmpty())
        result["id"] = id;
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(result).toJson()));
}

void DebugServer::startLiveSubscription(QWebSocket *socket, const QString &id,
                                        DebugCommand command, int intervalMs)
{
    // Stop any existing subscription with the same id on this socket.
    stopLiveSubscription(socket, id);

    auto *timer = new QTimer(socket);
    timer->setInterval(intervalMs);

    // Each tick creates a fresh one-shot session — no compositor connection
    // persists between ticks.
    connect(timer, &QTimer::timeout, this, [this, socket, command, id]() {
        QJsonObject result;
        if (command == DebugCommand::Top) {
            result = sessionRequest([this](Session &session) {
                QList<ClientInfo> clients;
                if (!waitSlot(session.replica->getClients(), m_timeoutMs, &clients))
                    return QJsonObject{{"ok", false}, {"error", "getClients() failed"}};
                qint64 focusId = 0, cursorId = 0;
                waitSlot(session.replica->focusedWindowId(), m_timeoutMs, &focusId);
                waitSlot(session.replica->windowUnderCursor(), m_timeoutMs, &cursorId);
                return QJsonObject{
                    {"ok", true},
                    {"data", QJsonObject{
                        {"clients", clientsToJson(clients)},
                        {"focusedWindowId", focusId},
                        {"cursorWindowId", cursorId}
                    }}
                };
            });
        } else if (command == DebugCommand::Events) {
            const quint64 since = socket->property("eventsLastSeq").toULongLong();
            result = sessionRequest([this, since, socket](Session &session) {
                QList<DebugEvent> events;
                if (!waitSlot(session.replica->getEvents(since), m_timeoutMs, &events))
                    return QJsonObject{{"ok", false}, {"error", "getEvents() failed"}};
                quint64 lastSeq = since;
                for (const auto &e : events)
                    lastSeq = std::max(lastSeq, e.seq());
                socket->setProperty("eventsLastSeq", lastSeq);
                return QJsonObject{{"ok", true}, {"data", debugEventsToJson(events)}};
            });
        } else if (command == DebugCommand::Watch) {
            const qint64 windowId = socket->property("watchId").toLongLong();
            result = sessionRequest([this, windowId](Session &session) {
                QList<WindowInfo> windows;
                if (!waitSlot(session.replica->getWindows(), m_timeoutMs, &windows))
                    return QJsonObject{{"ok", false}, {"error", "getWindows() failed"}};
                for (const auto &w : windows) {
                    if (w.id() == windowId)
                        return QJsonObject{{"ok", true}, {"data", windowToJson(w)}};
                }
                return QJsonObject{{"ok", false}, {"error", "window no longer exists"}};
            });
        }

        result["id"] = id;
        result["subscription"] = true;
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(result).toJson()));
    });

    // Store the timer so we can stop it later.
    const QByteArray key = "sub_" + id.toUtf8();
    socket->setProperty(key.constData(), QVariant::fromValue(timer));

    // Clean up when the socket is destroyed.
    connect(socket, &QWebSocket::destroyed, timer, &QTimer::deleteLater);

    timer->start();
}

void DebugServer::stopLiveSubscription(QWebSocket *socket, const QString &id)
{
    // If id is empty, stop all subscriptions on this socket.
    const auto names = socket->dynamicPropertyNames();
    const QByteArray prefix = id.isEmpty() ? "sub_" : ("sub_" + id.toUtf8());
    for (const QByteArray &name : names) {
        if (!name.startsWith("sub_"))
            continue;
        if (!id.isEmpty() && name != prefix)
            continue;
        auto *timer = socket->property(name.constData()).value<QTimer *>();
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
        socket->setProperty(name.constData(), QVariant());
    }
}

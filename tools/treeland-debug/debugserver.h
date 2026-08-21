// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef TREELAND_DEBUG_SERVER_H
#define TREELAND_DEBUG_SERVER_H

#include "debugsession.h"

#include <QHttpServer>
#include <QHttpServerResponse>
#include <QObject>
#include <QString>

// HTTP + WebSocket server exposing every treeland-debug capability over the
// network. Each HTTP request (and each WebSocket one-shot command) creates its
// own Session — connecting to the compositor's remote object only for the
// duration of that request, then tearing it down — so the server consumes no
// compositor-side resources while idle. Live WebSocket subscriptions (top,
// events, watch) hold a Session open only for the lifetime of the subscription.
class DebugServer : public QObject
{
    Q_OBJECT

public:
    explicit DebugServer(const QString &url, const QString &name, int timeoutMs,
                         QObject *parent = nullptr);

    // Binds the HTTP and WebSocket listener to @p host:@p port.  Returns true
    // on success.
    bool listen(const QString &host, int port);

private:
    // Creates and connects a fresh one-shot Session.  Returns true on success.
    bool createSession(Session &session);

    // Helper: runs a Session-scoped RPC and returns the JSON result object
    // {ok, data} or {ok:false, error}.  Used by HTTP handlers that don't need
    // custom serialization.
    QJsonObject sessionRequest(
        const std::function<QJsonObject(Session &)> &work);

    // --- HTTP route handlers (inspection) ---
    QJsonObject handleTree();
    QJsonObject handleCursor();
    QJsonObject handleWindows();
    QJsonObject handleClients();
    QJsonObject handleFocused();
    QJsonObject handleCursorWindow();

    // --- HTTP route handlers (window control) ---
    QJsonObject handleActivate(const QHttpServerRequest &request);
    QJsonObject handleClose(const QHttpServerRequest &request);
    QJsonObject handleMinimize(const QHttpServerRequest &request);
    QJsonObject handleMaximize(const QHttpServerRequest &request);
    QJsonObject handleFullscreen(const QHttpServerRequest &request);
    QJsonObject handleMove(const QHttpServerRequest &request);
    QJsonObject handleResize(const QHttpServerRequest &request);
    QJsonObject handleWorkspace(const QHttpServerRequest &request);

    // --- HTTP route handlers (input / events) ---
    QJsonObject handleMoveCursor(const QHttpServerRequest &request);
    QJsonObject handleEventMotion(const QHttpServerRequest &request);
    QJsonObject handleEventButton(const QHttpServerRequest &request);
    QJsonObject handleEventKey(const QHttpServerRequest &request);
    QJsonObject handleEvents(const QHttpServerRequest &request);

    // --- HTTP route handlers (image capture, return raw PNG) ---
    QHttpServerResponse handleScreenshotOutput(const QHttpServerRequest &request);
    QHttpServerResponse handleScreenshotWindow(const QHttpServerRequest &request);

    // --- WebSocket handling ---
    void onNewWebSocketConnection();
    void handleWebSocketMessage(class QWebSocket *socket, const QString &message);
    void startLiveSubscription(QWebSocket *socket, const QString &id,
                               DebugCommand command, int intervalMs);
    void stopLiveSubscription(QWebSocket *socket, const QString &id);

    QString m_url;
    QString m_name;
    int m_timeoutMs;
    QHttpServer m_httpServer;
};

#endif // TREELAND_DEBUG_SERVER_H

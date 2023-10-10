// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wcursor.h>
Q_MOC_INCLUDE("wquickoutputlayout.h")

#include <QQmlEngine>
#include <QQmlParserStatus>

QT_BEGIN_NAMESPACE
class QQmlComponent;
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WQuickCursorAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WCursor::CursorShape shape READ shape WRITE setShape NOTIFY shapeChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WQuickCursorAttached(QQuickItem *parent);

    QQuickItem *parent() const;

    WCursor::CursorShape shape() const;
    void setShape(WCursor::CursorShape shape);

Q_SIGNALS:
    void shapeChanged();
};

class WQuickOutputLayout;
class WOutputRenderWindow;
class WQuickCursorPrivate;
class WAYLIB_SERVER_EXPORT WQuickCursor : public WCursor, public QQmlParserStatus
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickCursor)
    Q_PROPERTY(WQuickOutputLayout* layout READ layout WRITE setLayout REQUIRED)
    Q_PROPERTY(WOutputRenderWindow* currentRenderWindow READ currentRenderWindow NOTIFY currentRenderWindowChanged)
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY themeNameChanged)
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged)
    QML_NAMED_ELEMENT(Cursor)
    QML_ATTACHED(WQuickCursorAttached)
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit WQuickCursor(QObject *parent = nullptr);
    ~WQuickCursor();

    static WQuickCursorAttached *qmlAttachedProperties(QObject *target);

    WQuickOutputLayout *layout() const;
    void setLayout(WQuickOutputLayout *layout);

    WOutputRenderWindow *currentRenderWindow() const;

    QString themeName() const;
    void setThemeName(const QString &name);

    QSize size() const;
    void setSize(const QSize &size);

Q_SIGNALS:
    void themeNameChanged();
    void sizeChanged();
    void currentRenderWindowChanged();

private:
    using WCursor::setLayout;
    using WCursor::layout;

    void classBegin() override;
    void componentComplete() override;

    W_PRIVATE_SLOT(void updateRenderWindows())
    W_PRIVATE_SLOT(void updateCurrentRenderWindow())
    W_PRIVATE_SLOT(void updateXCursorManager())
};

WAYLIB_SERVER_END_NAMESPACE

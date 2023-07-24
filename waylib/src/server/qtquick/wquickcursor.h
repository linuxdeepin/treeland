// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wcursor.h>
Q_MOC_INCLUDE("wquickoutputlayout.h")

#include <QQmlEngine>
#include <QQmlParserStatus>

QT_BEGIN_NAMESPACE
class QQmlComponent;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

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
    QML_NAMED_ELEMENT(Cursor)
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit WQuickCursor(QObject *parent = nullptr);
    ~WQuickCursor();

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

    void move(QW_NAMESPACE::QWInputDevice *device, const QPointF &delta) override;
    void setPosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos) override;
    bool setPositionWithChecker(QW_NAMESPACE::QWInputDevice *device, const QPointF &pos) override;
    void setScalePosition(QW_NAMESPACE::QWInputDevice *device, const QPointF &ratio) override;

    void classBegin() override;
    void componentComplete() override;

    W_PRIVATE_SLOT(void updateRenderWindows())
    W_PRIVATE_SLOT(void updateXCursorManager())
};

WAYLIB_SERVER_END_NAMESPACE

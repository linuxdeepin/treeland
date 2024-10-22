// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "gestures.h"

#include <QObject>

class TogglableGesture : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool inProgress READ inProgress WRITE setInProgress NOTIFY inProgressChanged FINAL)
    Q_PROPERTY(qreal partialGestureFactor READ partialGestureFactor WRITE setPartialGestureFactor NOTIFY partialGestureFactorChanged FINAL)
    Q_PROPERTY(Status status READ status WRITE setStatus NOTIFY statusChanged FINAL)
    Q_PROPERTY(qreal desktopOffset READ desktopOffset NOTIFY desktopOffsetChanged)
public:
    enum Status
    {
        Inactive,
        Activating,
        Deactivating,
        Active,
        Stopped
    };

    Q_ENUM(Status);

    TogglableGesture(QObject *parent = nullptr);
    ~TogglableGesture();

    bool inProgress() const;
    void setInProgress(bool gesture);

    void setPartialGestureFactor(qreal factor);

    qreal partialGestureFactor() const
    {
        return m_partialGestureFactor;
    }

    void setStatus(Status status);

    Status status() const
    {
        return m_status;
    }

    void activate();
    void deactivate();

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void stop();

    void setDesktopOffset(qreal offset);

    qreal desktopOffset() const
    {
        return m_desktopOffset;
    }

    void addTouchpadSwipeGesture(SwipeGesture::Direction direction, uint fingerCount);
Q_SIGNALS:
    void inProgressChanged();
    void partialGestureFactorChanged(qreal factor);
    void activated();
    void deactivated();
    void statusChanged(Status status);
    void desktopOffsetChanged();
    void desktopOffsetCancelled();

protected:
    std::function<void(qreal progress)> progressCallback();
    std::function<void(qreal progress)> regressCallback();
    void setProgress(qreal progress);
    void setRegress(qreal regress);

    std::function<void()> activeTriggeredCallback();
    std::function<void()> deactivateTriggeredCallback();
    void activeTriggered();
    void deactivateTriggered();

private:
    Status m_status = Status::Inactive;
    bool m_inProgress = false;
    qreal m_partialGestureFactor;
    qreal m_desktopOffset;
    bool m_desktopOffsetRelevant = false;
};

Q_DECLARE_METATYPE(TogglableGesture *)

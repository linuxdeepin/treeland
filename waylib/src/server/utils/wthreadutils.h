/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <wglobal.h>

#include <QCoreApplication>
#include <QPointer>
#include <QThread>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WThreadUtil {
public:
    WThreadUtil(QObject *eventObject, QEvent::Type eventType)
        : eventObject(eventObject)
        , eventType(eventType)
    {

    }

    typedef std::function<void()> FunctionType;
    class CallEvent : public QEvent {
    public:
        CallEvent(QEvent::Type type)
            : QEvent(type) {}

        FunctionType function;
        QPointer<QObject> target;
    };

    template <typename ReturnType>
    class _TMP
    {
    public:
        Q_ALWAYS_INLINE static void run(WThreadUtil *self, QObject *target, std::function<void()> fun)
        {
            if (Q_UNLIKELY(QThread::currentThread() == self->eventObject->thread())) {
                fun();
                return;
            }

            CallEvent *event = new CallEvent(self->eventType);
            event->target = target;
            event->function = fun;
            QCoreApplication::postEvent(self->eventObject, event);
        }

        template <typename T>
        Q_ALWAYS_INLINE static typename std::enable_if<!std::is_base_of<QObject, T>::value, void>::type
        run(WThreadUtil *self, T *, std::function<void()> fun)
        {
            return run(self, static_cast<QObject*>(nullptr), fun);
        }
    };

    template <typename Fun, typename... Args>
//    inline typename QtPrivate::FunctionPointer<Fun>::ReturnType
    Q_ALWAYS_INLINE
    void run(QObject *target, typename QtPrivate::FunctionPointer<Fun>::Object *obj, Fun fun, Args&&... args)
    {
        return _TMP<typename QtPrivate::FunctionPointer<Fun>::ReturnType>::run(this, target, std::bind(fun, obj, std::forward<Args>(args)...));
    }
    template <typename Fun, typename... Args>
//    inline typename QtPrivate::FunctionPointer<Fun>::ReturnType
    Q_ALWAYS_INLINE
    void run(typename QtPrivate::FunctionPointer<Fun>::Object *obj, Fun fun, Args&&... args)
    {
        return _TMP<typename QtPrivate::FunctionPointer<Fun>::ReturnType>::run(this, obj, std::bind(fun, obj, std::forward<Args>(args)...));
    }
    template <typename Fun, typename... Args>
    Q_ALWAYS_INLINE /*auto*/
    typename std::enable_if<std::is_invocable<typename std::remove_pointer<Fun>::type>::value, void>::type
    run(QObject *target, Fun fun, Args&&... args) /*-> decltype(fun(args...))*/
    {
        return _TMP<decltype(fun(args...))>::run(this, target, std::bind(fun, std::forward<Args>(args)...));
    }
    template <typename Fun, typename... Args>
    Q_ALWAYS_INLINE /*auto*/
    typename std::enable_if<std::is_invocable<typename std::remove_pointer<Fun>::type>::value, void>::type
    run(Fun fun, Args&&... args) /*-> decltype(fun(args...))*/
    {
        return run(static_cast<QObject*>(nullptr), fun, std::forward<Args>(args)...);
    }

protected:
    QObject *eventObject;
    QEvent::Type eventType;
};

WAYLIB_SERVER_END_NAMESPACE

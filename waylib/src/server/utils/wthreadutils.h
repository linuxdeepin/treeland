// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QCoreApplication>
#include <QPointer>
#include <QThread>
#include <QFuture>
#include <QEvent>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WThreadUtil final
{
public:
    explicit WThreadUtil(QThread *thread);
    ~WThreadUtil();

    static const WThreadUtil &gui();

    QThread *thread() const noexcept;

    template <typename Func, typename... Args>
    inline auto run(const QObject *context, typename QtPrivate::FunctionPointer<Func>::Object *obj, Func fun, Args&&... args) const
    {
        return call(context, fun, *obj, std::forward<Args>(args)...);
    }
    template <typename Func, typename... Args>
    inline auto run(typename QtPrivate::FunctionPointer<Func>::Object *obj, Func fun, Args&&... args) const
    {
        if constexpr (std::is_base_of<QObject, typename QtPrivate::FunctionPointer<Func>::Object>::value) {
            return call(obj, fun, *obj, std::forward<Args>(args)...);
        } else {
            return call(static_cast<QObject*>(nullptr), fun, *obj, std::forward<Args>(args)...);
        }
    }
    template <typename Func, typename... Args>
    inline QFuture<std::invoke_result_t<std::decay_t<Func>, Args...>>
    run(const QObject *context, Func fun, Args&&... args) const
    {
        return call(context, fun, std::forward<Args>(args)...);
    }
    template <typename Func, typename... Args>
    inline QFuture<std::invoke_result_t<std::decay_t<Func>, Args...>>
    run(Func fun, Args&&... args) const
    {
        return call(static_cast<QObject*>(nullptr), fun, std::forward<Args>(args)...);
    }
    template <typename... T> inline auto exec(T&&... args) const
    {
        auto future = run(std::forward<T>(args)...);
        if (!thread()->isRunning()) {
            qWarning() << "The target thread is not running, maybe lead to deadlock.";
        }
        future.waitForFinished();

        if constexpr (std::is_same_v<decltype(future), QFuture<void>>) {
            return;
        } else {
            return future.result();
        }
    }

private:
    class AbstractCallEvent : public QEvent {
    public:
        AbstractCallEvent(QEvent::Type type) : QEvent(type) {}
        virtual void call() = 0;
    };

    template <typename Func, typename... Args>
    class Q_DECL_HIDDEN CallEvent : public AbstractCallEvent {
        typedef QtPrivate::FunctionPointer<std::decay_t<Func>> FunInfo;
        typedef std::invoke_result_t<std::decay_t<Func>, Args...> ReturnType;

    public:
        CallEvent(QEvent::Type type, Func &&fun, Args&&... args)
            : AbstractCallEvent(type)
            , function(fun)
            , arguments(std::forward<Args>(args)...)
        {

        }

        QEvent *clone() const override {
            return nullptr;
        }

        void call() override {
            if (promise.isCanceled()) {
                return;
            }

            if (contextChecker == context) {
                promise.start();
#ifndef QT_NO_EXCEPTIONS
                try {
#endif
                    if constexpr (std::is_void<ReturnType>::value) {
                        std::apply(function, arguments);
                    } else {
                        promise.addResult(std::apply(function, arguments));
                    }
#ifndef QT_NO_EXCEPTIONS
                } catch (...) {
                    promise.setException(std::current_exception());
                }
#endif
                promise.finish();
            } else {
                promise.start();
                promise.setException(std::make_exception_ptr(
                    std::runtime_error("The context object is destroyed.")));
                promise.finish();
            }
        }

        Func function;
        const std::tuple<Args...> arguments;
        QPromise<ReturnType> promise;

        const QObject *context;
        QPointer<const QObject> contextChecker;
    };

    template <typename Func, typename... Args>
    auto call(const QObject *context, Func fun, Args&&... args) const {
        typedef QtPrivate::FunctionPointer<std::decay_t<Func>> FunInfo;
        typedef std::invoke_result_t<std::decay_t<Func>, Args...> ReturnType;

        if constexpr (FunInfo::IsPointerToMemberFunction) {
            static_assert(std::is_same_v<std::decay_t<typename QtPrivate::List<Args...>::Car>, typename FunInfo::Object>,
                          "The obj and function are not compatible.");
            static_assert(QtPrivate::CheckCompatibleArguments<typename QtPrivate::List<Args...>::Cdr, typename FunInfo::Arguments>::value,
                          "The args and function are not compatible.");
        } else if constexpr (FunInfo::ArgumentCount != -1) {
            static_assert(QtPrivate::CheckCompatibleArguments<QtPrivate::List<Args...>, typename FunInfo::Arguments>::value,
                          "The args and function are not compatible.");
        } else {
            using Prototype = ReturnType(*)(Args...);
            QtPrivate::AssertCompatibleFunctions<Prototype, Func>();
        }

        QPromise<ReturnType> promise;
        auto future = promise.future();

        if (Q_UNLIKELY(QThread::currentThread() == m_thread)) {
            promise.start();
            if constexpr (std::is_void<ReturnType>::value) {
                std::invoke(fun, std::forward<Args>(args)...);
            } else {
                promise.addResult(std::invoke(fun, std::forward<Args>(args)...));
            }
            promise.finish();
        } else {
            auto event = new CallEvent<Func, Args...>(eventType, std::move(fun), std::forward<Args>(args)...);
            event->promise = std::move(promise);
            event->context = context;
            event->contextChecker = context;

            QCoreApplication::postEvent(ensureThreadContextObject(), event);
        }

        return future;
    }

    QObject *ensureThreadContextObject() const;

    friend class Caller;
    static QEvent::Type eventType;
    QThread *m_thread;
    mutable QAtomicPointer<QObject> threadContext;
};

WAYLIB_SERVER_END_NAMESPACE

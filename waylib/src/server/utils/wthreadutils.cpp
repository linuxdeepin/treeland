// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wthreadutils.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

QEvent::Type WThreadUtil::eventType = static_cast<QEvent::Type>(QEvent::registerEventType());

class Q_DECL_HIDDEN Caller : public QObject
{
public:
    explicit Caller()
        : QObject()
    {
    }

    bool event(QEvent *event) override
    {
        if (event->type() == WThreadUtil::eventType) {
            auto ev = static_cast<WThreadUtil::AbstractCallEvent*>(event);
            ev->call();
            return true;
        }

        return QObject::event(event);
    }
};

WThreadUtil::WThreadUtil(QThread *thread)
    : m_thread(thread)
    , threadContext(nullptr)
{

}

WThreadUtil::~WThreadUtil()
{
    delete threadContext.loadRelaxed();
}

const WThreadUtil &WThreadUtil::gui()
{
    static auto *global = new WThreadUtil(QCoreApplication::instance()->thread());
    return *global;
}

QThread *WThreadUtil::thread() const noexcept
{
    return m_thread;
}

QObject *WThreadUtil::ensureThreadContextObject() const
{
    QObject *context;
    if (!threadContext.loadRelaxed()) {
        context = new Caller();
        context->moveToThread(m_thread);
        if (!threadContext.testAndSetRelaxed(nullptr, context)) {
            context->moveToThread(nullptr);
            delete context;
        }
    }

    context = threadContext.loadRelaxed();
    Q_ASSERT(context);

    return context;
}

WAYLIB_SERVER_END_NAMESPACE

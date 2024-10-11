// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "cmdline.h"

#include <QCommandLineOption>
#include <QCommandLineParser>

#include <optional>

CmdLine::CmdLine()
    : QObject()
    , m_parser(std::make_unique<QCommandLineParser>())
    , m_socket(std::make_unique<QCommandLineOption>(
          QStringList{ "s", "socket" }, "set ddm socket", "socket"))
    , m_run(std::make_unique<QCommandLineOption>(QStringList{ "r", "run" }, "run a process", "run"))
    , m_lockScreen(std::make_unique<QCommandLineOption>("lockscreen",
                                                        "use lockscreen, need DDM auth socket"))
{
    m_parser->addHelpOption();
    m_parser->addOptions({ *m_socket.get(), *m_run.get(), *m_lockScreen.get() });
    m_parser->process(*QCoreApplication::instance());
}

std::optional<QString> CmdLine::socket() const
{
    if (m_parser->isSet(*m_socket.get())) {
        return m_parser->value(*m_socket.get());
    }

    return std::nullopt;
}

std::optional<QString> CmdLine::run() const
{
    if (m_parser->isSet(*m_run.get())) {
        return m_parser->value(*m_run.get());
    }

    return std::nullopt;
}

bool CmdLine::useLockScreen() const
{
    return m_parser->isSet(*m_lockScreen.get());
}

// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <DSingleton>

#include <QObject>
#include <QCommandLineOption>

#include <memory>
#include <optional>

class QCoreApplication;
class QCommandLineParser;

DCORE_USE_NAMESPACE

class CmdLine
    : public QObject
    , public DSingleton<CmdLine>
{
    Q_OBJECT
    friend class DSingleton<CmdLine>;

public:
    std::optional<QString> run() const;
    bool useLockScreen() const;
    std::optional<QStringList> unescapeExecArgs(const QString &str) noexcept;
    bool tryExec() const;

private:
    CmdLine();
    ~CmdLine();
    QString unescape(const QString &str) noexcept;

private:
    std::unique_ptr<QCommandLineParser> m_parser;
    std::unique_ptr<QCommandLineOption> m_run;
    std::unique_ptr<QCommandLineOption> m_lockScreen;
    QCommandLineOption m_tryExec;
};

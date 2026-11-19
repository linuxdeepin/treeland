// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QString>
#include <QStringList>

// Line editor for the interactive shell. Reads stdin in raw mode so a single
// keypress can be acted on: Tab completes commands, arrows move the cursor and
// browse history, Ctrl+C discards the line, Ctrl+D ends the session. The
// terminal is put back into its original mode before readLine() returns, so
// live views (top/events/watch) still see Ctrl+C as SIGINT.
//
// Only used when stdin and stdout are both a terminal; a piped line keeps going
// through QTextStream::readLine(), which needs no editing.
class ShellLineEditor
{
public:
    // Reads one line without its line ending. Returns false on EOF (Ctrl+D on
    // an empty line, or stdin closed).
    bool readLine(const QString &prompt, QString *line);

private:
    QStringList m_history;
};

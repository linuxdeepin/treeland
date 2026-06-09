// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <QStringList>
class QCoreApplication;

int runService(QCoreApplication &app, const QStringList &args);
int runCtl(QCoreApplication &app, const QStringList &args);

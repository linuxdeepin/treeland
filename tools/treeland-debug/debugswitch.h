// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QList>
#include <QString>

#include <sys/types.h>
#include <unistd.h> // geteuid(), used by the callers of treelandDebugTargetUid()

// Treeland's `remoteDebug` master switch, shared by the `debug` command and the
// `listen` HTTP API so both read and write the same key.

// uids of every running treeland, deduplicated and sorted. Read through /proc,
// which needs no privileges: the answer is the same before and after a polkit
// escalation, so the elevated copy writes the key of the instance the caller
// meant.
QList<uid_t> treelandCompositorUids();

QString treelandUserName(uid_t uid);

// Picks the user whose per-user switch key matters: our own when a compositor
// runs as us, the only running compositor otherwise. Sets *error when several
// compositors run as other users -- the switch is per user, so the caller has to
// name one with dde-dconfig itself.
bool treelandDebugTargetUid(uid_t *uid, QString *error);

// Reads the switch of @p uid (want = -1) or writes it (want = 1 on, 0 off),
// returning the value read back in *enabled. A write is verified by reading the
// key again, so a DConfig daemon that accepted but ignored it is an error.
// Escalation is the caller's job: DConfig only accepts a plain user's own key,
// so writing another user's key needs root.
// The call blocks on dde-dconfig: @p timeoutMs bounds each of the (up to two)
// invocations, so a caller running on an event loop others depend on -- the
// `listen` server -- passes a short one.
bool treelandRemoteDebugSwitch(uid_t uid, int want, bool *enabled, QString *error,
                               int timeoutMs = 30000);

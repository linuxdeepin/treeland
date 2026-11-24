// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wglobal.h>

WAYLIB_SERVER_USE_NAMESPACE

void setAttachedDataFromOtherTranslationUnit(WObject *object, int *data)
{
    object->setAttachedData<int>(data);
}
